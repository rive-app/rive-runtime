// Parallel calls on a page with threads. A team of pool threads runs a
// module's parts on members: instances of the module that share its memory,
// made without its start or its data, which would undo what the page's own
// instance already did to that memory. A call hands its parts to the team
// through words in shared memory, and the page's thread runs its own part
// while the members run theirs. The page's thread cannot wait, so it spins.
// Team threads spin a while after each call, then park in a way that still
// lets them take the memory of a module loaded later.

import {
    kParallelArgs,
    kParallelArrived,
    kParallelAtOnce,
    kParallelCallerParks,
    kParallelClaim,
    kParallelCount,
    kParallelGeneration,
    kParallelParts,
    kParallelSleepers,
    kParallelStop,
    kParallelThunk,
} from './rive_lane_imports.mjs';

// The team's words, by Int32 index. Words that threads race on sit on cache
// lines of their own.
const kTeamCall = 0;
const kTeamParked = 16;
const kTeamThreads = 32;
const kTeamVm = 33;
const kTeamControl = 34;
const kTeamGlobals = 35;
const kTeamFailed = 48;
const kTeamFailKind = 49;
const kTeamFailAt = 50;
const kTeamFailLength = 51;
const kTeamFailMs = 52;
const kTeamFailBytes = 53;
const kTeamWaits = 54;
// Each thread's line: the call it last finished, the call the page assigned
// it until the thread takes its part, its part in that call, and the last
// call it saw. The page writes an assignment before the call starts, so a
// thread that wakes late never takes a later call's part as its own, and
// clears one the thread never took, so it never runs it after the call ends.
const kTeamDone = 128;
const kTeamAssigned = 1;
const kTeamPart = 2;
const kTeamSeen = 3;
// As many as a native call's team.
const kTeamMax = 63;
// The most parts a call gets by default, one per core up to this.
const kDefaultParts = 8;
const kTeamTextAt = (kTeamDone + 16 * kTeamMax) * 4;
const kTeamTextBytes = 512;

const kFailTrap = 1;
const kFailThrow = 2;
const kFailBudget = 3;

const kWaitAsync = 1;
const kWaitTimed = 2;

// How long a team thread polls for the next call before it parks, as the
// native team does, so a frame's next stage starts without a wake.
const kSpinMs = 0.05;
// A parked thread without Atomics.waitAsync looks for messages this often.
const kTimedWaitMs = 2;
// How long a call waits for a thread to take its part before it wakes the
// team again, and before it gives up on the thread.
const kRewakeMs = 1;
const kTakeMs = 1000;
// How long preparing a module waits for its members.
const kReadyMs = 10000;
// As each native member's spill zone, which fills the frame collector's
// 64 KiB raw block after its header.
const kSpillBytes = 65536 - 16;

const textOf = (error) => String(error?.message ?? error);

// Polls done until it holds or about ms pass, reading the clock only now and
// then since a read costs more than a poll.
function spinFor(done, ms) {
    const until = performance.now() + ms;
    for (let polls = 1; ; polls++) {
        if (done()) {
            return true;
        }
        if ((polls & 63) === 0 && performance.now() >= until) {
            return false;
        }
    }
}

// One event loop turn, so a parked thread takes its messages.
function nextTurn() {
    const channel = new MessageChannel();
    return new Promise((resolve) => {
        channel.port1.onmessage = () => {
            channel.port1.close();
            resolve();
        };
        channel.port2.postMessage(0);
    });
}

// What a member may import: the clock, aligned to the page's so a deadline
// the page set means the same here, the throw and budget reports a part's
// failure carries, debug probes, which only matter where a debugger runs
// everything on the page's thread, and the check in only part 0 makes.
// Anything else the build refused.
function memberImports(module, memory, state, offset) {
    const imports = {};
    const stopped = () => state.view !== null && Atomics.load(state.view, state.stop) !== 0;
    const known = {
        'rive_rt_v1': {
            'now': () => {
                // A fuel check reads the clock, so a stopped call ends there.
                if (stopped()) {
                    throw new Error('stopped by another part');
                }
                return performance.now() + offset;
            },
            'budget_exceeded': (ms) => {
                state.budgetMs = ms;
                throw new Error('execution exceeded timeout');
            },
            'error': (at, length) => {
                state.thrown = [at, length];
            },
            'debug_enter': () => {},
            'debug_line': () => 0,
            'debug_leave': () => {},
        },
        'rive_parallel_v1': {
            'waiting': () => {},
        },
    };
    for (const wanted of WebAssembly.Module.imports(module)) {
        const space = (imports[wanted['module']] ??= {});
        if (wanted['kind'] === 'memory') {
            space[wanted['name']] = memory;
        } else if (wanted['kind'] === 'function') {
            space[wanted['name']] =
                known[wanted['module']]?.[wanted['name']] ??
                (() => {
                    throw new Error(
                        `parallel code called the host through ${wanted['module']}.${wanted['name']}`
                    );
                });
        }
    }
    return imports;
}

// The team thread's side: its members, by module, and its loop.
function teamThread() {
    const members = new Map();
    let words = null;
    let index = -1;

    function addMember(data) {
        const ready = new Int32Array(data['ready']);
        const state = { thrown: null, budgetMs: -1, view: null, stop: 0 };
        try {
            // Each thread compiles its own, as threads that lazily compile
            // one shared module at once have crashed the engine.
            const module = new WebAssembly.Module(data['bytes']);
            const memory = data['memory'];
            const offset = performance.timeOrigin - data['origin'];
            const instance = new WebAssembly.Instance(
                module,
                memberImports(module, memory, state, offset)
            );
            const exports = instance.exports;
            // A module whose code never spills exports no spill floor.
            const spill = exports['__riveSpillFloor'] === undefined ? 0 : data['spill'];
            if (spill !== 0) {
                exports['__riveSpillFloor'].value = spill;
            }
            members.set(data['key'], { exports, memory, state, spill, buffer: null, view: null });
            Atomics.store(ready, index, 1);
        } catch (error) {
            Atomics.store(ready, index, 2);
        }
        Atomics.notify(ready, index);
    }

    // The first failure of the call wins; it stops the other parts at their
    // next check or sync.
    function fail(participant, member, error) {
        if (Atomics.compareExchange(words, kTeamFailed, -1, participant) !== -1) {
            return;
        }
        const state = member.state;
        if (state.thrown !== null) {
            words[kTeamFailKind] = kFailThrow;
            words[kTeamFailAt] = state.thrown[0];
            words[kTeamFailLength] = state.thrown[1];
        } else if (state.budgetMs >= 0) {
            words[kTeamFailKind] = kFailBudget;
            words[kTeamFailMs] = state.budgetMs;
        } else {
            words[kTeamFailKind] = kFailTrap;
            const text = new TextEncoder().encode(textOf(error)).subarray(0, kTeamTextBytes);
            new Uint8Array(words.buffer, kTeamTextAt, text.length).set(text);
            words[kTeamFailBytes] = text.length;
        }
        Atomics.store(member.view, member.state.stop, 1);
    }

    function runShare(generation) {
        const line = kTeamDone + 16 * index;
        if (Atomics.compareExchange(words, line + kTeamAssigned, generation, 0) !== generation) {
            return false;
        }
        const participant = words[line + kTeamPart];
        const threads = words[kTeamThreads];
        const member = members.get(words[kTeamVm]);
        if (member === undefined) {
            return true;
        }
        // The page grows the memory only between calls.
        const buffer = member.memory.buffer;
        if (member.buffer !== buffer) {
            member.buffer = buffer;
            member.view = new Int32Array(buffer);
        }
        const view = member.view;
        const control = words[kTeamControl] >>> 0;
        const state = member.state;
        state.thrown = null;
        state.budgetMs = -1;
        state.view = view;
        state.stop = (control + kParallelStop) >>> 2;
        const exports = member.exports;
        // A trap skips the epilogues that give back spill space.
        if (member.spill !== 0) {
            exports['__riveCallSpill'].value = member.spill + kSpillBytes;
        }
        try {
            exports['__riveLoadGlobals'](words[kTeamGlobals]);
        } catch (error) {
            fail(participant, member, error);
            return true;
        }
        const count = view[(control + kParallelCount) >>> 2];
        const thunk = view[(control + kParallelThunk) >>> 2];
        const args = view[(control + kParallelArgs) >>> 2];
        const parts = view[(control + kParallelParts) >>> 2] >>> 2;
        const call = exports['__riveParallelCall'];
        for (let part = participant; part < count; part += threads) {
            if (Atomics.load(view, state.stop) !== 0) {
                break;
            }
            try {
                call(thunk, view[parts + part], args);
            } catch (error) {
                fail(participant, member, error);
                break;
            }
        }
        return true;
    }

    async function loop() {
        // Tests under node keep a thread from taking parts with
        // RIVE_PARALLEL_DEAF_THREAD, as one whose wake was lost would.
        if (String(index) === globalThis.process?.['env']?.['RIVE_PARALLEL_DEAF_THREAD']) {
            return;
        }
        let seen = Atomics.load(words, kTeamCall);
        const moved = () => Atomics.load(words, kTeamCall) !== seen;
        // WebKit can lose the wake of an async wait, which stalls a call
        // until it gives up on the thread. Tests under node take the timed
        // path with RIVE_PARALLEL_TIMED_WAITS.
        const userAgent = globalThis.navigator?.userAgent ?? '';
        const waitAsync =
            typeof Atomics.waitAsync === 'function' &&
            !(userAgent.includes('AppleWebKit/') && !userAgent.includes('Chrome/')) &&
            globalThis.process?.['env']?.['RIVE_PARALLEL_TIMED_WAITS'] === undefined;
        Atomics.store(words, kTeamWaits, waitAsync ? kWaitAsync : kWaitTimed);
        let turned = performance.now();
        for (;;) {
            if (!spinFor(moved, kSpinMs)) {
                Atomics.add(words, kTeamParked, 1);
                if (waitAsync) {
                    const waited = Atomics.waitAsync(words, kTeamCall, seen);
                    if (waited.async) {
                        await waited.value;
                    }
                } else if (performance.now() - turned >= kTimedWaitMs) {
                    // Messages get a turn in place of a sleep, so a woken
                    // call never waits on one.
                    await nextTurn();
                    turned = performance.now();
                } else {
                    Atomics.wait(words, kTeamCall, seen, kTimedWaitMs);
                }
                Atomics.sub(words, kTeamParked, 1);
                continue;
            }
            seen = Atomics.load(words, kTeamCall);
            Atomics.store(words, kTeamDone + 16 * index + kTeamSeen, seen);
            if (runShare(seen)) {
                Atomics.store(words, kTeamDone + 16 * index, seen);
            }
        }
    }

    return {
        // The page's messages to this thread; false for any other.
        message(data) {
            const team = data?.['riveParallelTeam'];
            if (team !== undefined) {
                words = new Int32Array(team['words']);
                index = team['index'];
                loop();
                return true;
            }
            const vm = data?.['riveParallelVm'];
            if (vm !== undefined) {
                addMember(vm);
                return true;
            }
            const drop = data?.['riveParallelDrop'];
            if (drop !== undefined) {
                members.delete(drop);
                return true;
            }
            return false;
        },
    };
}

// The page's side: the team, which every module on the page shares, one call
// at a time, and each module's members on it. threads.start() starts a team
// thread and answers its worker, or null where there are no threads.
export function createParallel(threads) {
    let team = null;
    let nextKey = 1;
    const thread = teamThread();
    // The call the page's thread is in.
    let current = null;

    // Team threads to size - 1, started once and kept.
    function teamOf(size) {
        team ??= {
            words: new Int32Array(
                new SharedArrayBuffer(kTeamTextAt + kTeamTextBytes)
            ),
            workers: [],
            // By thread, the last call it saw when a call gave up on it, or
            // undefined; it sits out calls until it sees another.
            lost: [],
        };
        while (team.workers.length + 1 < Math.min(size, kTeamMax + 1)) {
            const index = team.workers.length;
            const worker = threads.start();
            if (worker === null) {
                break;
            }
            worker.postMessage({ 'riveParallelTeam': { 'words': team.words.buffer, 'index': index } });
            team.workers.push(worker);
        }
        return team;
    }

    // Members on the team for a module whose memory is shared, once its own
    // instance has started, from the bytes each member compiles. Null when
    // there is no team to run them.
    function setUp(entry, bytes, size) {
        const exports = entry.instance.exports;
        if (
            size <= 1 ||
            !threads.can() ||
            exports['__riveParallelCall'] === undefined ||
            exports['__riveSaveGlobals'] === undefined ||
            exports['__riveRawAlloc'] === undefined
        ) {
            return null;
        }
        const { workers } = teamOf(size);
        if (workers.length === 0) {
            return null;
        }
        // The allocator traps once the memory cannot grow. As natively, the
        // members made before then stay, and without any a call runs alone.
        const alloc = (bytes) => {
            try {
                return exports['__riveRawAlloc'](bytes) >>> 0;
            } catch {
                return 0;
            }
        };
        const globals = alloc(Math.max(exports['__riveGlobalBytes'].value, 4));
        if (globals === 0) {
            return null;
        }
        const parallel = {
            key: nextKey++,
            globals,
            ready: new Int32Array(new SharedArrayBuffer(4 * workers.length)),
            running: false,
        };
        workers.forEach((worker, index) => {
            const spill = alloc(kSpillBytes);
            if (spill === 0) {
                Atomics.store(parallel.ready, index, 2);
                return;
            }
            worker.postMessage({
                'riveParallelVm': {
                    'key': parallel.key,
                    'bytes': bytes,
                    'memory': entry.memory,
                    'spill': spill,
                    'ready': parallel.ready.buffer,
                    'origin': performance.timeOrigin,
                },
            });
        });
        return parallel;
    }

    // Settles once every member of the module started or failed to, or a
    // team thread that never answers is left out. It polls on timers, as a
    // pending waitAsync would not keep node running.
    async function ready(parallel) {
        const until = performance.now() + kReadyMs;
        while (parallel.ready.some((state) => state === 0) && performance.now() < until) {
            await new Promise((done) => setTimeout(done, 1));
        }
    }

    function release(parallel) {
        for (const worker of team?.workers ?? []) {
            worker.postMessage({ 'riveParallelDrop': parallel.key });
        }
    }

    // Whether a thread a call gave up on has since seen another call, as one
    // that only woke late has.
    function present(index) {
        const lost = team.lost[index];
        if (lost === undefined) {
            return true;
        }
        if (Atomics.load(team.words, kTeamDone + 16 * index + kTeamSeen) === lost) {
            return false;
        }
        team.lost[index] = undefined;
        return true;
    }

    // For a member that has not taken its part: once the call fails, the
    // page takes the part back. Until then the page wakes the team again,
    // since the member may have missed its wake, and fails the call once the
    // member is too late.
    function nudge(call, index, now) {
        const words = team.words;
        const line = kTeamDone + 16 * index;
        if (Atomics.load(words, line + kTeamAssigned) !== call.generation) {
            return;
        }
        if (Atomics.load(call.view, call.stop) === 0 && now - call.began >= kTakeMs) {
            call.fail(new Error(
                `a thread of the page's team did not start its part of a parallel call within ${kTakeMs} ms`
            ));
        }
        if (Atomics.load(call.view, call.stop) !== 0) {
            if (Atomics.compareExchange(words, line + kTeamAssigned, call.generation, 0) === call.generation) {
                team.lost[index] = Atomics.load(words, line + kTeamSeen);
                call.taken.add(index);
            }
        } else if (now - call.woke >= kRewakeMs) {
            call.woke = now;
            Atomics.notify(words, kTeamCall);
        }
    }

    // Waits for each member to finish its part, or for the page to take it
    // back. A call past its budget fails here too.
    function join(call, fuelCheck) {
        const words = team.words;
        for (const index of call.members) {
            const line = kTeamDone + 16 * index;
            for (let polls = 1; Atomics.load(words, line) !== call.generation; polls++) {
                if ((polls & 1023) !== 0) {
                    continue;
                }
                if (call.taken.has(index)) {
                    break;
                }
                if (Atomics.load(call.view, call.stop) === 0 && fuelCheck !== undefined) {
                    try {
                        fuelCheck();
                    } catch (error) {
                        call.fail(error);
                    }
                }
                nudge(call, index, performance.now());
            }
        }
    }

    // Part 0 checking in from a sync, where the page's thread waits inside
    // the module and the join cannot run.
    function waiting() {
        const call = current;
        if (call === null) {
            return;
        }
        const now = performance.now();
        for (const index of call.members) {
            nudge(call, index, now);
        }
    }

    // Runs the call whose control block is at control, one part per thread
    // it gets when the call asks for every part at once, and answers the
    // threads that ran parts. report raises a member's throw or budget
    // failure as the page's own instance would.
    function run(entry, parallel, control, report) {
        if (parallel?.running) {
            throw new Error('parallel calls cannot nest');
        }
        // A word an atomic reads must be aligned.
        if ((control & 3) !== 0) {
            throw new Error('parallel call control block is not aligned');
        }
        // The page grows the memory only between calls.
        const buffer = entry.memory.buffer;
        if (entry.view32?.buffer !== buffer) {
            entry.view32 = new Int32Array(buffer);
        }
        const view = entry.view32;
        const word = (offset) => (control + offset) >>> 2;
        let count = view[word(kParallelCount)];
        const members = [];
        for (let index = 0; index < (parallel?.ready.length ?? 0) && members.length + 1 < count; index++) {
            if (Atomics.load(parallel.ready, index) === 1 && present(index)) {
                members.push(index);
            }
        }
        const threads = members.length + 1;
        if (view[word(kParallelAtOnce)] !== 0) {
            count = threads;
            view[word(kParallelCount)] = count;
        }
        view[word(kParallelAtOnce)] = threads >= count ? 1 : 0;
        // The claim takes two words, and the page's thread may not park.
        for (const offset of [
            kParallelGeneration,
            kParallelArrived,
            kParallelSleepers,
            kParallelClaim,
            kParallelClaim + 4,
            kParallelStop,
            kParallelCallerParks,
        ]) {
            Atomics.store(view, word(offset), 0);
        }
        const exports = entry.instance.exports;
        const call = exports['__riveParallelCall'];
        const thunk = view[word(kParallelThunk)];
        const args = view[word(kParallelArgs)];
        const parts = view[word(kParallelParts)] >>> 2;
        if (threads === 1) {
            for (let part = 0; part < count; part++) {
                call(thunk, view[parts + part], args);
            }
            return 1;
        }
        const words = team.words;
        parallel.running = true;
        try {
            exports['__riveSaveGlobals'](parallel.globals);
            words[kTeamThreads] = threads;
            words[kTeamVm] = parallel.key;
            words[kTeamControl] = control;
            words[kTeamGlobals] = parallel.globals;
            Atomics.store(words, kTeamFailed, -1);
            // An assignment of 0 is none.
            const generation = ((Atomics.load(words, kTeamCall) + 1) | 0) || 1;
            members.forEach((index, j) => {
                const line = kTeamDone + 16 * index;
                words[line + kTeamPart] = j + 1;
                Atomics.store(words, line + kTeamAssigned, generation);
            });
            Atomics.store(words, kTeamCall, generation);
            if (Atomics.load(words, kTeamParked) !== 0) {
                Atomics.notify(words, kTeamCall);
            }
            let thrown = null;
            const began = performance.now();
            current = {
                members,
                generation,
                view,
                stop: word(kParallelStop),
                began,
                woke: began,
                // Members whose parts the page took back.
                taken: new Set(),
                // The page's own failure, which stops the other parts unless
                // a member failed first.
                fail(error) {
                    thrown ??= error;
                    if (Atomics.compareExchange(words, kTeamFailed, -1, 0) === -1) {
                        Atomics.store(view, word(kParallelStop), 1);
                    }
                },
            };
            for (let part = 0; part < count; part += threads) {
                if (Atomics.load(view, word(kParallelStop)) !== 0) {
                    break;
                }
                try {
                    call(thunk, view[parts + part], args);
                } catch (error) {
                    current.fail(error);
                    break;
                }
            }
            join(current, exports['__riveFuelCheck']);
            const failed = Atomics.load(words, kTeamFailed);
            if (failed > 0) {
                const kind = words[kTeamFailKind];
                if (kind === kFailThrow) {
                    report.thrown(words[kTeamFailAt], words[kTeamFailLength]);
                } else if (kind === kFailBudget) {
                    report.budget(words[kTeamFailMs]);
                    if (thrown !== null) {
                        throw thrown;
                    }
                }
                const text = new Uint8Array(words[kTeamFailBytes]);
                text.set(new Uint8Array(words.buffer, kTeamTextAt, text.length));
                throw new Error(
                    kind === kFailTrap ? new TextDecoder().decode(text) : 'parallel part trapped'
                );
            }
            if (thrown !== null) {
                throw thrown;
            }
            return threads;
        } finally {
            parallel.running = false;
            current = null;
        }
    }

    return {
        setUp,
        ready,
        release,
        run,
        waiting,
        message: thread.message,
        // The parts a call gets: chosen, where the host chose a number, else
        // the page's default.
        parts(chosen) {
            const parts = Math.trunc(chosen ?? NaN);
            return Number.isFinite(parts)
                ? Math.min(Math.max(parts, 1), kTeamMax + 1)
                : Math.min(globalThis.navigator?.hardwareConcurrency ?? 1, kDefaultParts);
        },
        // How the team waits when parked, for tests to report.
        state() {
            const waits = team === null ? 0 : Atomics.load(team.words, kTeamWaits);
            return {
                'team': team?.workers.length ?? 0,
                'waits': waits === kWaitAsync ? 'async' : waits === kWaitTimed ? 'timed' : 'none',
            };
        },
    };
}
