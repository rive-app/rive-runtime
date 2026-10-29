#!/bin/bash
# Builds librive for the web with the wasm scripting backend and runs baked
# AnimaScript files on the JS engine under node, the browser lane's path.
set -e
cd "$(dirname "$0")"

# The link does not track its --pre-js input, so a changed bundle would
# otherwise be silently left out.
for BUNDLE in ../../src/wasm/web/rive_{scripting,audio_worklet,threads}_pre.js; do
    for LINKED in out/wasm{,_threads}_debug/web_scripting_{harness,page}; do
        if [[ $BUNDLE -nt $LINKED.mjs ]]; then
            rm -f $LINKED.mjs $LINKED.wasm
        fi
    done
done

# A lane that never ends holds node's event loop, so a run that would wait on
# one fails under this alarm instead of hanging the suite.
guarded() {
    perl -e 'alarm shift; exec @ARGV' 60 "$@"
}

# The audio worklet's shared memory protocol, on a heap laid out by hand.
node shared_lane_test.mjs
# The page side of the lanes and of real time audio, without librive.
guarded node web_host_test.mjs
# The browser audio runner when a browser exits early or cannot start.
node browser_audio_test.mjs

build_rive.sh wasm debug --with_rive_scripting --scripting_vm=wasm \
    --with_rive_layout --with_rive_tools -- web_scripting_harness \
    web_scripting_page

HARNESS=out/wasm_debug/web_scripting_harness.mjs
BASIC=(--expect "init ran on the web" --expect "clocks ok" --expect "random ok"
    --expect "drew frame 2")
node run.mjs $HARNESS fixture/basic.riv "${BASIC[@]}"
# Without prepare() the module instantiates inline when scripts start.
node run.mjs $HARNESS fixture/basic.riv --sync "${BASIC[@]}"
# A host without a list of its files services every script VM through the
# runner once a frame, as the JS runtime does, which hands back warnings only.
node run.mjs $HARNESS fixture/basic.riv --page-boundary \
    --expect "page boundary called __riveFrameMinor" \
    --expect "page boundary warned []"
# A parallel module runs its parts one after another where there are no
# threads, with its memory shared or, without cross origin isolation,
# unshared. Its first call comes in its first step, which once trapped. Its
# second call throws in a part, and the call after it starts clean.
PARALLEL=(--until "parallel after a throw"
    --expect "parallel fill wrong 0" --expect "parallel structs wrong 0"
    --expect "parallel inplace ok" --expect "parallel globals ok"
    --expect "parallel sync ok" --expect "parallel up to 2 ok"
    --expect "parallel fixed parts ok" --expect "parallel claims wrong 0"
    --expect "parallel exchange ok" --expect "parallel clock wrong 0"
    --expect "parallel memory grew yes" --expect "parallel grown wrong 0"
    --expect "wasm call trapped in host_obj_advance: the last part gave up"
    --expect "parallel after a throw ok")
SERIAL=(--expect "parallel size 1" --expect "parallel threads no"
    --expect "parallel parts all yes")
# Every part spins until the call's budget stops them all.
STOPPED=(--until "parallel after a stop"
    --expect "wasm call trapped in host_obj_advance: execution exceeded timeout"
    --expect "parallel after a stop ok")
# Parallel.inPart() is false on the main lane and true in parts, a PerPart
# grows to a call's parts before they run, and parts call a @parallel
# function value bit for bit as the main lane does.
DESIGN=(--until "design friction wrong" --expect "design in part ok"
    --expect "design per part wrong 0 objects enough"
    --expect "design friction wrong 0")
for MODE in "" --sync --no-isolation; do
    node run.mjs $HARNESS fixture/parallel.riv $MODE "${SERIAL[@]}" "${PARALLEL[@]}"
    node run.mjs $HARNESS fixture/parallel_design.riv $MODE "${DESIGN[@]}"
done
node run.mjs $HARNESS fixture/parallel_stop.riv "${STOPPED[@]}"
# Without threads a job runs at a later advance, as a budgeted call never runs
# one on its joiner, which rive-cli's script_jobs_test.cpp also asserts.
JOBS=(--expect "job threads 0" --expect "job square 9.0"
    --expect "job tile munich 4 1.5 munich/inner" --expect "job error bad code 3"
    --expect "job says 42" --expect "job matches 24"
    --expect "unjoined job done 144.0" --expect "job says 7"
    --until "job matches" --until "job says 7")
node run.mjs $HARNESS fixture/jobs.riv "${JOBS[@]}"
node run.mjs $HARNESS fixture/jobs.riv --sync "${JOBS[@]}"
# A job may run in the instance an earlier job left on its thread, and finds
# it as a fresh one. A job that threw or grew its memory far leaves none.
REUSE=(--until "reuse done" --expect "reuse fresh state 32 of 32"
    --expect "reuse kept an instance yes"
    --expect "reuse after a throw fresh yes: tripped 3"
    --expect "reuse after a stop fresh yes"
    --expect "reuse dropped the hungry instance yes")
ANEW=(--expect "reuse after a throw started anew yes")
node run.mjs $HARNESS fixture/job_reuse.riv "${REUSE[@]}" "${ANEW[@]}"
node run.mjs $HARNESS fixture/job_reuse.riv --sync "${REUSE[@]}" "${ANEW[@]}"
# Shared memory through jobs: atomics, the lock, a ring, a snapshot, and a
# trap inside exclusive that poisons and frees its region.
SHARED=(--until "shared lock freed" --expect "shared atomic 800 locked 400"
    --expect "shared ring 20 sum 190.0" --expect "shared snapshot 4.0"
    --expect "shared trap failed poisoned yes" --expect "shared lock freed"
    --expect "shared rebuilt handle held its own"
    --expect "shared value after release 7.0")
node run.mjs $HARNESS fixture/shared.riv "${SHARED[@]}"
# Workers without threads step between frames on the page's thread, and the
# main lane only watches them, so the run lets frames pass until they finish.
# The native lane asserts the same lines.
WORKERS=(--until "workers done" --expect "worker echo 10 last 20.0 count 10"
    --expect "worker echo stopped finished" --expect "worker sleep completed"
    --expect "worker threw failed with code" --expect "worker thrower failed: worker code 7"
    --expect "worker adders agree yes" --expect "worker snapshot 5"
    --expect "workers done")
node run.mjs $HARNESS fixture/workers.riv "${WORKERS[@]}"
node run.mjs $HARNESS fixture/workers.riv --sync "${WORKERS[@]}"
# A worker that awaits again after its stop is ended, and one that yields
# each pass gets a bounded slice per advance, so neither holds the page.
STOPS=(--until "worker stops done" --expect "ticker stopped yielder finished"
    --expect "worker stops done")
node run.mjs $HARNESS fixture/worker_stops.riv "${STOPS[@]}"
node run.mjs $HARNESS fixture/worker_stops.riv --sync "${STOPS[@]}"
# Workers without threads step again while due within an advance's slice,
# and a parked one lets its host settle, as the native lane asserts.
SLICE=(--until "worker slice done"
    --expect "yielder rounds past one per advance yes"
    --expect "worker slice done" --expect "pending async work no")
node run.mjs $HARNESS fixture/worker_slice.riv "${SLICE[@]}"
node run.mjs $HARNESS fixture/worker_slice.riv --sync "${SLICE[@]}"
# Workers meet at a barrier whose participant count changes, and a break or a
# stop settles every call false, as the native lane asserts.
BARRIER=(--until "barrier worker 0 met" --until "barrier worker 1 met"
    --until "barrier worker 2 met" --until "barrier worker 3 met"
    --until "barrier break settled" --until "barrier stop settled"
    --until "barrier stop settles a lone participant"
    --expect "barrier worker 0 met 1056 rounds, 0 wrong"
    --expect "barrier worker 1 met 1056 rounds, 0 wrong"
    --expect "barrier worker 2 met 432 rounds, 0 wrong"
    --expect "barrier worker 3 met 432 rounds, 0 wrong"
    --expect "barrier break settled 3 waits, 1 at a gate, and 3 later calls false"
    --expect "barrier stop settled 3 waits, 1 at a gate, false"
    --expect "barrier stop settles a lone participant false"
    --expect "barrier released settles false")
node run.mjs $HARNESS fixture/barrier.riv "${BARRIER[@]}"
node run.mjs $HARNESS fixture/barrier.riv --sync "${BARRIER[@]}"
# Only advances step a worker without threads, so its timed wait keeps a host
# that settles advancing until it wakes.
node run.mjs $HARNESS fixture/worker_timed.riv --while-pending \
    --expect "sleeper woke"
# A lane without threads holds the page's thread, so a metered job or worker
# step that never ends fails at the page's call budget and frames go on.
SPINNER_TIMEOUT="execution exceeded timeout on the page's thread"
for FIXTURE in job_spin job_spin_fuel; do
    guarded node run.mjs $HARNESS fixture/$FIXTURE.riv --until "job after" \
        --expect "spinner failed: $SPINNER_TIMEOUT" \
        --expect "job after the spinners 9.0"
done
for FIXTURE in worker_spin worker_spin_fuel; do
    guarded node run.mjs $HARNESS fixture/$FIXTURE.riv --error-sink \
        --until "spinner running" \
        --expect-line "error sink: worker spinner failed: $SPINNER_TIMEOUT"
done
# The main lane awaits jobs, workers, signals and sleeps, and a worker's
# promise hands back its value, as the native lane asserts.
AWAITS=(--until "awaits done" --expect "joined promise Pending" --expect "promise square 16.0"
    --expect "joined promise settled 25.0" --expect "promise rejected bad code 3"
    --expect "main signal 1.0" --expect "main signal timed out yes"
    --expect "main sleep completed" --expect "awaited square 36.0 then slept yes"
    --expect "worker result answer 4 6.0" --expect "worker void done"
    --expect "worker rejected with code" --expect "worker peek 6.0"
    --expect "awaits done")
node run.mjs $HARNESS fixture/awaits.riv "${AWAITS[@]}"
# Ports to a service worker: replies awaited and drained, a full port
# refusing, a blob outliving its senders, as the native lane asserts.
PORTS=(--until "ports done" --expect "port full refused yes"
    --expect "port awaited 30.0" --expect "cancelled job promise: job was stopped"
    --expect "port drained 3 total 21.0"
    --expect "port filled 20 total 190.0 refused some yes"
    --expect "port blob sum 19900.0" --expect "port service answered 4"
    --expect "ports done")
# Without threads a worker only steps between advances, so a wait inside one
# never sees its answer.
node run.mjs $HARNESS fixture/ports.riv "${PORTS[@]}" \
    --expect "port answered within the call never"
# Audio processors render blocks on the engine's clock from their messages
# alone, so the page and the native lane must hear the same samples.
AUDIO=(--until "processor running" --audio 4 --expect "ramp init 2 channels"
    --expect "processor 0 state 1 sums 496.32 -496.32"
    --expect "processor 1 state 3 sums 0.00 0.00"
    --expect "processor Thrower failed: bad block" --expect "processor threw bad block"
    --expect "processor meter 512.0 gain 2.0")
node run.mjs $HARNESS fixture/audio.riv "${AUDIO[@]}"
node run.mjs $HARNESS fixture/audio.riv --sync "${AUDIO[@]}"
# The same samples through the shared memory natives an audio worklet uses:
# Atomics on librive's heap rather than librive's own natives.
node run.mjs $HARNESS fixture/audio.riv --lane-twin "${AUDIO[@]}"
# A processor's float recurrences and v128 path render the same bits on every
# tier, with fuel or without, as rive-cli's script_jobs_test.cpp expects.
SAME_SAMPLES="sums -3.45 650.69 hash 12aca437"
SAME_BITS=(--until "bank running" --audio 16
    --expect "processor 0 state 1 $SAME_SAMPLES")
DETERMINISM=(determinism determinism_fuel determinism_release)
for FIXTURE in "${DETERMINISM[@]}"; do
    node run.mjs $HARNESS fixture/$FIXTURE.riv "${SAME_BITS[@]}"
done
# Without cross origin isolation nothing would play a processor, so its start
# fails saying why instead of running silent.
node run.mjs $HARNESS fixture/audio.riv --until "processor threw" \
    --expect "processor threw real time audio needs cross origin isolation" \
    --expect "processor Thrower failed: real time audio needs cross origin isolation"
# A worklet reaches librive's heap through Atomics, which a build without
# threads cannot share.
node run.mjs $HARNESS fixture/audio.riv --nrdp-mock --until "processor threw" \
    --expect "processor threw real time audio needs librive built with threads"
# A processor fails on a record naming shared objects and leaves it in the
# ring, through librive's natives or through Atomics.
HOLDS=(--pump-audio --until "ring keeps"
    --expect "reader failed: a processor cannot pass shared objects through rings or snapshots"
    --expect "ring keeps its record")
node run.mjs $HARNESS fixture/processor_holds.riv "${HOLDS[@]}"
node run.mjs $HARNESS fixture/processor_holds.riv --lane-twin "${HOLDS[@]}"
# A worker feeds a processor through a ring as the processor makes space.
FEED=(--pump-audio --expect "feed received 4096 sum 8386560.0")
node run.mjs $HARNESS fixture/feed.riv "${FEED[@]}"
node run.mjs $HARNESS fixture/feed.riv --lane-twin "${FEED[@]}"
# A blob crosses to jobs and processors as a handle; a job makes one for its
# result, and a processor plays one through librive or through Atomics. Each
# lane copies in and out by range.
BLOB=(--pump-audio --expect "blob bytes 1000 sum 125316.0"
    --expect "blob part 50 read 30 sum 3525.0"
    --expect "blob job sum 125316.0 slices 125316.0"
    --expect "blob made 1000 doubled 125704.0"
    --expect "sampler played 4096 sum 4193280.0")
node run.mjs $HARNESS fixture/blob.riv "${BLOB[@]}"
node run.mjs $HARNESS fixture/blob.riv --lane-twin "${BLOB[@]}"
# The main lane's audio clock is the frames the harness has pulled, so a note
# stamped against it plays on its frame, as in a rive-cli render.
CLOCK=(--pump-audio --expect "clock starts at 0.0" --expect "clock offset 0.0"
    --expect "clock played 0.0 frames late")
node run.mjs $HARNESS fixture/clock.riv "${CLOCK[@]}"
# GPU buffers take ranges of a job's blob from librive's heap, by update and
# at their make, and the recording keeps the bytes once the blob is gone.
GPU_BLOB=(--until "gpu upload" --expect "skin uploaded"
    --expect "gpu upload at 8 of 32 bytes sum 3024"
    --expect "gpu buffer of 8 bytes immutable sum 660")
node run.mjs $HARNESS fixture/gpu_blob.riv "${GPU_BLOB[@]}"

# The same jobs on pthreads, the lane a cross origin isolated page runs: pool
# threads are browser workers, and only a pool thread can finish the job no
# one joins, since nothing pumps the queue when threads exist.
RIVE_OUT=out/wasm_threads_debug build_rive.sh wasm debug --with_rive_scripting \
    --scripting_vm=wasm --with_rive_layout --with_rive_tools --web_threads \
    -- web_scripting_harness web_scripting_page
THREADED=out/wasm_threads_debug/web_scripting_harness.mjs
# A build with threads refuses at once to load where it cannot share memory.
REFUSED="harness refused: this build runs threads, which need shared memory, and the page is not cross origin isolated; load the build without threads"
node run.mjs $THREADED fixture/basic.riv --no-isolation --expect "$REFUSED"
# A parallel call on pthreads runs one part on each of the page's team
# threads, as many as riveScripting.parallelParts asks with the page's own.
# The V8 in node 26 races its tier up of a call into JS against other threads
# making the same call, which browsers do not, so these runs leave that tier
# up off.
PARALLEL_NODE=(node --wasm-wrapper-tiering-budget=2000000000 run.mjs $THREADED)
TEAM=(--parallel-parts 4 --expect "parallel size 4" --expect "parallel threads yes")
"${PARALLEL_NODE[@]}" fixture/parallel.riv "${TEAM[@]}" "${PARALLEL[@]}" \
    --expect "parallel parts all yes" --expect 'parallel team {"team":3,"waits":"async"}'
# Started without prepare(), the team starts in the background, and calls
# before it has get fewer parts.
"${PARALLEL_NODE[@]}" fixture/parallel.riv --sync "${TEAM[@]}" "${PARALLEL[@]}"
# A parked team thread without Atomics.waitAsync still takes its messages.
RIVE_PARALLEL_TIMED_WAITS=1 "${PARALLEL_NODE[@]}" fixture/parallel.riv \
    "${TEAM[@]}" "${PARALLEL[@]}" --expect "parallel parts all yes" \
    --expect 'parallel team {"team":3,"waits":"timed"}'
"${PARALLEL_NODE[@]}" fixture/parallel.riv --parallel-safe "${SERIAL[@]}" \
    "${PARALLEL[@]}"
# One part turns parallel calls off for the page.
"${PARALLEL_NODE[@]}" fixture/parallel.riv --parallel-parts 1 "${SERIAL[@]}" \
    "${PARALLEL[@]}" --expect 'parallel team {"team":0,"waits":"none"}'
"${PARALLEL_NODE[@]}" fixture/parallel_stop.riv --parallel-parts 4 "${STOPPED[@]}"
"${PARALLEL_NODE[@]}" fixture/parallel_design.riv --parallel-parts 4 "${DESIGN[@]}"
# A team thread that never takes its part, as one whose wake was lost, fails
# the call within its budget, or after a second without one, rather than
# hanging the page, and later calls run without it. A syncing call fails the
# same way.
LOST=(--until "parallel after a lost thread"
    --expect "parallel after a lost thread parts 3 total 3")
export RIVE_PARALLEL_DEAF_THREAD=1
guarded "${PARALLEL_NODE[@]}" fixture/parallel_lost.riv --parallel-parts 4 "${LOST[@]}" \
    --expect "wasm call trapped in host_obj_advance: execution exceeded timeout"
guarded "${PARALLEL_NODE[@]}" fixture/parallel_lost.riv --parallel-parts 4 "${LOST[@]}" \
    --call-budget 0 --expect "wasm call trapped in host_obj_advance: a thread of the page's team did not start its part of a parallel call within 1000 ms"
guarded "${PARALLEL_NODE[@]}" fixture/parallel_frames.riv --parallel-parts 4 \
    --expect "wasm call trapped in host_obj_advance: execution exceeded timeout"
# Part 0 waits at its sync on the page's thread, where the join cannot run,
# so it checks in with the page, and without a budget the call fails within a
# second there too.
perl -e 'alarm shift; exec @ARGV' 10 "${PARALLEL_NODE[@]}" fixture/parallel_frames.riv \
    --parallel-parts 4 --call-budget 0 --expect "wasm call trapped in host_obj_advance: a thread of the page's team did not start its part of a parallel call within 1000 ms"
unset RIVE_PARALLEL_DEAF_THREAD
node run.mjs $THREADED fixture/jobs.riv "${JOBS[@]:2}"
# The page compiles each lane module once and hands it to the pool's threads,
# with or without prepare(), so none of them compiles its own.
for MODE in "" --sync; do
    node --import ./worker_compiles.mjs run.mjs $THREADED fixture/jobs.riv \
        $MODE "${JOBS[@]:2}" --expect "worker threads compiled 0 modules"
done
# Each pool thread keeps its own instance, so which job finds one varies.
node run.mjs $THREADED fixture/job_reuse.riv "${REUSE[@]}"
# Nothing interrupts the page's thread, so its join ends with the call's
# budget, and a cancel reaches a job through the flag it polls, fuel or not.
for FIXTURE in job_cancel job_cancel_release; do
    node run.mjs $THREADED fixture/$FIXTURE.riv --until "spinner " \
        --expect "long join ended within a second yes" \
        --expect "spinner cancelled: job was stopped"
done
# A stopped job that never polls ends at its next fuel check, freeing its
# pool thread for the next job, as releasing its file does.
SPINNERS=(--until "job after" --expect "spinners stopped yes"
    --expect "job after the spinners 9.0")
for FIXTURE in job_spin job_spin_fuel; do
    node run.mjs $THREADED fixture/$FIXTURE.riv "${SPINNERS[@]}"
done
node run.mjs $THREADED fixture/jobs.riv --first fixture/job_spin_fuel.riv "${JOBS[@]:2}"
node run.mjs $THREADED fixture/shared.riv "${SHARED[@]}"
# A lock that ends with a top level's budget moves a metered module's fuel on,
# which takes a call into the module while it starts.
for MODE in "" --sync; do
    node run.mjs $THREADED fixture/top_lock.riv $MODE \
        --expect "top level lock timed out turns 2000"
done
# Workers on a small pool of pthreads.
node run.mjs $THREADED fixture/workers.riv "${WORKERS[@]}"
node run.mjs $THREADED fixture/worker_stops.riv "${STOPS[@]}"
node run.mjs $THREADED fixture/worker_slice.riv "${SLICE[@]}"
node run.mjs $THREADED fixture/barrier.riv "${BARRIER[@]}"
node run.mjs $THREADED fixture/awaits.riv "${AWAITS[@]}"
# A worker on its own pthread answers while the page's thread waits.
node run.mjs $THREADED fixture/ports.riv "${PORTS[@]}" \
    --expect "port answered within the call 7.0"
# A stopped worker that never awaits again ends at a fuel check past its grace.
# One without fuel runs on, and its file's release waits only the grace for it.
SPIN=(--until "spinner running" --expect "spinner stop sent")
for FIXTURE in worker_spin worker_spin_fuel; do
    node run.mjs $THREADED fixture/$FIXTURE.riv "${SPIN[@]}" \
        --expect "spinner running after its stop no"
done
node run.mjs $THREADED fixture/worker_spin_release.riv "${SPIN[@]}" \
    --expect "spinner running after its stop yes"
# A spinning worker must not starve the workers its runner stepped before,
# as the native lane asserts.
node run.mjs $THREADED fixture/worker_pinned.riv \
    --until "pinned workers progress" --expect "pinned workers progress yes"
# Init runs on a pool thread, but the instance starts where blocks render.
node run.mjs $THREADED fixture/audio.riv "${AUDIO[@]}"
node run.mjs $THREADED fixture/audio.riv --lane-twin "${AUDIO[@]}"
for FIXTURE in "${DETERMINISM[@]}"; do
    node run.mjs $THREADED fixture/$FIXTURE.riv "${SAME_BITS[@]}"
done
# The feeder runs on its own pthread, so the processor's read, through
# librive or through Atomics, must wake it across threads.
node run.mjs $THREADED fixture/feed.riv "${FEED[@]}"
node run.mjs $THREADED fixture/feed.riv --lane-twin "${FEED[@]}"
node run.mjs $THREADED fixture/blob.riv "${BLOB[@]}"
node run.mjs $THREADED fixture/blob.riv --lane-twin "${BLOB[@]}"
node run.mjs $THREADED fixture/gpu_blob.riv "${GPU_BLOB[@]}"
# NRDP's audio API, mocked as we measured it on devices, drives the runner's
# NRDP output and the worklet bundle NRDP's thread runs.
# These blocks render in real time, so no hard limit keeps a loaded machine
# from dropping a processor the run needs.
STEADY="--hard-blocks 0"
node run.mjs $THREADED fixture/audio.riv --nrdp-mock $STEADY --until "processor meter" \
    --expect "ramp init 2 channels" --expect "processor running" \
    --expect "processor threw bad block" --expect "processor meter"
node run.mjs $THREADED fixture/feed.riv --nrdp-mock $STEADY --until "feed received" \
    --expect "feed received 4096 sum 8386560.0"
node run.mjs $THREADED fixture/processor_holds.riv --nrdp-mock $STEADY "${HOLDS[@]:1}"
for FIXTURE in overrun overrun_release; do
    node run.mjs $THREADED fixture/$FIXTURE.riv --nrdp-mock --until "slow failed" \
        --expect "slow failed: processor overran its block budget"
done
# A metered block stuck past its budget ends at its next fuel check and the
# mix plays on, as does one whose init never returns, offline too.
for FIXTURE in worklet_stall worklet_stall_fuel; do
    guarded node run.mjs $THREADED fixture/$FIXTURE.riv --nrdp-mock --hard-blocks 20 \
        --until "quiet rendered" \
        --expect "stuck failed: processor overran its block budget" \
        --expect "quiet rendered after stuck failed"
done
INIT_SPIN=(--until "quiet rendered"
    --expect "spinner 0 failed: processor init overran its budget"
    --expect "spinner 1 failed: processor init overran its budget"
    --expect "quiet rendered after the spinners failed")
guarded node run.mjs $THREADED fixture/init_spin.riv --nrdp-mock $STEADY "${INIT_SPIN[@]}"
guarded node run.mjs $HARNESS fixture/init_spin.riv --pump-audio "${INIT_SPIN[@]}"
# The worklet bundle renders the bits harness_audio hashes.
for FIXTURE in "${DETERMINISM[@]}"; do
    node run.mjs $THREADED fixture/$FIXTURE.riv --nrdp-mock $STEADY --hash 16 \
        --until "worklet rendered" --expect "worklet rendered 16 blocks $SAME_SAMPLES"
done
# A processor stuck in a block freezes NRDP's audio, so the page ends the
# thread, frees the lock and lane word it held and fails the processor.
node run.mjs $THREADED fixture/stall.riv --nrdp-stall $STEADY \
    --until "stall ring written after" \
    --expect "stall failed: processor stalled the audio thread" \
    --expect "stall ring written after yes" \
    --expect "stall thread terminated yes" --expect "stall lock freed yes" \
    --expect "stall lane freed yes"
# A worklet script that cannot be fetched, or a thread that never starts or
# never answers, fails the processors waiting on it.
node run.mjs $THREADED fixture/audio.riv --nrdp-fault start --until "processor threw" \
    --expect "processor threw script audio has no worklet: thread start refused"
node run.mjs $THREADED fixture/audio.riv --nrdp-fault silent --until "processor threw" \
    --expect "processor threw script audio worklet never answered"
node run.mjs $THREADED fixture/audio.riv --nrdp-fault fetch --until "processor threw" \
    --expect "processor threw script audio has no worklet: rive_audio_worklet_nrdp.js answered HTTP 404"
# The worklet keeps only the modules the page has not told it to forget.
node run.mjs $THREADED fixture/audio.riv --nrdp-mock $STEADY --until "processor meter" \
    --forget-modules --expect "processor meter" \
    --expect "forgot modules 9000 9001 9002" --expect "forgotten module resent yes"
# A worklet node that refuses librive's memory fails the start.
node run.mjs $THREADED fixture/audio.riv --worklet-throws --until "processor threw" \
    --expect "processor threw script audio has no worklet: memory could not be cloned"
# The worklet bundle runs a processor's relaxed SIMD, and a page whose engine
# lacks it turns the file's scripts off before any reaches the worklet.
node run.mjs $THREADED fixture/simd_processor.riv --nrdp-mock $STEADY \
    --until "processor relaxed lane" --expect "processor relaxed lane 7"
node run.mjs $THREADED fixture/simd_processor.riv --nrdp-mock --lacks relaxed --vms 0 \
    --expect "this browser lacks WebAssembly relaxed SIMD, so this file's scripts are off; bake with wasmSimd: on to support it" \
    --expect "artboard drew 5 frames"
# A gradient crosses as raw module addresses. The script then traps in its
# third advance, which is reported while the page keeps drawing.
node run.mjs $HARNESS fixture/trap.riv \
    --expect "gradient ready" \
    --expect "wasm call trapped in host_obj_advance" \
    --expect "last trap: unreachable" \
    --expect "drew after advance 3.0" \
    --expect "script vms alive after the frames: 1"
# A host's error sink takes the trap's line in place of stderr, as rive-cli's
# log does.
node run.mjs $HARNESS fixture/trap.riv --error-sink \
    --expect-line "error sink: wasm call trapped in host_obj_advance: unreachable"
# Gradient stops far past the module's memory trap the call, as on WAMR,
# rather than staging gigabytes.
node run.mjs $HARNESS fixture/gradient_bounds.riv \
    --expect "wasm call trapped in host_obj_user_init: out of bounds memory access" \
    --expect "script vms alive after the frames: 1"
# Module addresses past 2 GiB reach JS as negative i32s, yet the staged
# imports and librive's memory seam still reach them, as on WAMR.
node run.mjs $HARNESS fixture/high_memory.riv --seam 2147484416 \
    --expect "hi from past 2 GiB" \
    --expect-line "managed past 2 GiB" \
    --expect "trace: managed past 2 GiB 2" \
    --expect "high mat4 inverse 1 0.5" \
    --expect "high gradient minted" \
    --expect "seam seam"
# A full 4 GiB memory still counts all of its pages, and the collector still
# carves chunks below its top.
node run.mjs $HARNESS fixture/top_memory.riv --expect "top gradient minted" \
    --expect "top carve 300000"
# A trap leaves the spill zone to later calls: the runner puts the module's
# call state back after every call.
node run.mjs $HARNESS fixture/spill_trap.riv --expect "spill ok on frame 2"
# A collector that traps mid collection stops its module.
node run.mjs $HARNESS fixture/collector_trap.riv \
    --expect "wasm call trapped in __riveFrameMinor" \
    --expect "script vms alive after the frames: 0"
# Nesting that would run librive out of stack traps the call instead.
node run.mjs $HARNESS fixture/basic.riv --deep \
    --expect "last trap: script calls nest too deep for the host stack" \
    --expect "script vms alive after the frames: 1"
# A top level passes memory to the host, so its start function is lifted into
# an export. As on WAMR, only the ops that trap see the VM there.
TOP=(--expect "top level log" --expect "init after top level"
    --expect "top level path handle 0"
    --expect "top level join ran its job here no"
    --expect "top level job 9.0 region 8" --until "top level job")
node run.mjs $HARNESS fixture/top_level.riv "${TOP[@]}"
node run.mjs $HARNESS fixture/top_level.riv --sync "${TOP[@]}"
# A top level that traps inside exclusive leaves nothing held for the files
# the page loads after it.
for MODE in "" --sync; do
    for BUILD in $HARNESS $THREADED; do
        node run.mjs $BUILD fixture/top_level.riv $MODE \
            --first fixture/top_exclusive_trap.riv "${TOP[@]}" \
            --expect "module instantiate failed: threw inside exclusive"
    done
done
# A module whose start traps leaves the file, as a native one never joins it.
node run.mjs $HARNESS fixture/start_trap.riv --vms 0 \
    --expect "wasm script module failed to start: module instantiate failed" \
    --expect "script vms kept: 0"
# An error sink takes its line, as rive-cli's log does.
node run.mjs $HARNESS fixture/start_trap.riv --vms 0 --error-sink \
    --expect-line "error sink: wasm script module failed to start: module instantiate failed: unreachable"
# A host fault in a prepared module's start fails it and leaves librive's
# stack where it was.
node run.mjs $HARNESS fixture/start_fault.riv --vms 0 --fault raise \
    --expect "wasm script module failed to start: module instantiate failed: host fault injected" \
    --expect "librive stack kept across prepare"
# A metered module's own fuel stops a top level that would hang the page.
RUNAWAY=(--vms 0
    --expect "wasm script module failed to start: module instantiate failed: execution exceeded timeout")
node run.mjs $HARNESS fixture/runaway_top.riv "${RUNAWAY[@]}"
node run.mjs $HARNESS fixture/runaway_top.riv --sync "${RUNAWAY[@]}"
# A default release bake runs unbounded unless the page runs only what it can
# bound, as an editor does. Debug bakes run either way.
node run.mjs $HARNESS fixture/release.riv --expect "release init ran" \
    --expect "a runaway script will hang the page"
UNBOUNDED=(--bounded --vms 0
    --expect "wasm script module failed to start: module instantiate failed: this host runs only scripts it can bound"
    --expect-line "script modules instantiated: 0")
node run.mjs $HARNESS fixture/release.riv "${UNBOUNDED[@]}"
node run.mjs $HARNESS fixture/release.riv --sync "${UNBOUNDED[@]}"
node run.mjs $HARNESS fixture/basic.riv --bounded "${BASIC[@]}"
# A release bake with wasmFuel stops its own runaway loop.
METERED=(--expect "metered release init"
    --expect "wasm call trapped in host_obj_user_init: execution exceeded timeout")
node run.mjs $HARNESS fixture/release_fuel.riv "${METERED[@]}"
node run.mjs $HARNESS fixture/release_fuel.riv --sync "${METERED[@]}"
node run.mjs $HARNESS fixture/release_fuel.riv --bounded "${METERED[@]}"
# A bake whose main module has fuel meters its lanes too, so a page that runs
# only what it can bound runs its jobs as well.
node run.mjs $HARNESS fixture/jobs.riv --bounded "${JOBS[@]}"
node run.mjs $THREADED fixture/jobs.riv --bounded "${JOBS[@]:2}"
# An error sink takes the line its fuel reports and the failed start.
node run.mjs $HARNESS fixture/runaway_top.riv --vms 0 --error-sink \
    --expect-line "error sink: script error: execution exceeded 200 millisecond timeout" \
    --expect-line "error sink: wasm script module failed to start: module instantiate failed: execution exceeded timeout"
# Scripts started before the page prepared their module wait for it.
node run.mjs $HARNESS fixture/basic.riv --early "${BASIC[@]}" \
    --expect "wasm script module is still being prepared"
# Modules baked before rive_rt_v1 carried the clocks still read them.
node run.mjs $HARNESS fixture/legacy_clock.riv --expect "legacy clocks ok"
# A module importing an op no host has still starts, like on WAMR; only the
# call traps, naming the import.
UNLINKED=(--expect "init without the missing op"
    --expect "last trap: failed to call unlinked import function rive_web_gate_v1.missing")
node run.mjs $HARNESS fixture/unlinked.riv "${UNLINKED[@]}"
node run.mjs $HARNESS fixture/unlinked.riv --sync "${UNLINKED[@]}"
# This tools build links the raw shader op; runtime_wasm's own suite has no
# tools and checks that the call traps there.
node run.mjs $HARNESS fixture/raw_shader.riv \
    --expect "init reaches the raw shader op" \
    --expect "raw shader op returned 0"
# The browser decodes images for scripts, and its results arrive premultiplied
# on a later event loop turn.
DECODE=(--settle --expect "decoded 2x1 first pixel 100,128"
    --expect "decode failed: failed to decode image data")
node run.mjs $HARNESS fixture/decode.riv "${DECODE[@]}"
# A decoder that throws as librive starts it only fails that decode.
node run.mjs $HARNESS fixture/decode.riv --decoder-throws \
    --expect "decode failed: failed to decode image data" \
    --expect "script vms alive after the frames: 1"
# Scroll crosses as typed args, so the web gets it through the same call.
node run.mjs $HARNESS fixture/scroll.riv --scroll \
    --expect "scroll crossed intact" --expect "scroll claimed"
# console reaches the script log and keeps its timers on the web too.
node run.mjs $HARNESS fixture/console.riv \
    --expect "console log on the web" --expect "boot: " \
    --expect "Timer 'boot' does not exist"
# A null string logs an empty line, as on WAMR, rather than trapping, and one
# outside module memory traps the call as it does there.
node run.mjs $HARNESS fixture/console_null.riv \
    --expect "before the null log" --expect-line "" \
    --expect "after the null log" \
    --expect "wasm call trapped in host_obj_user_init: out of bounds memory access"
# Each lane's null string logs an empty line too and its job, worker and
# processor run on, on the page, on pthreads and in the worklet bundle.
LANE_NULL=(--expect-line "" --expect-line "" --expect-line ""
    --expect "job after the null log" --expect "worker after the null log"
    --expect "processor after the null log")
node run.mjs $HARNESS fixture/lane_console_null.riv --pump-audio "${LANE_NULL[@]}"
node run.mjs $THREADED fixture/lane_console_null.riv --pump-audio "${LANE_NULL[@]}"
node run.mjs $THREADED fixture/lane_console_null.riv --nrdp-mock $STEADY \
    --until "job after" --until "worker after" --until "processor after" \
    "${LANE_NULL[@]}"
# trace reaches the script log too, formatted as on WAMR.
node run.mjs $HARNESS fixture/trace.riv \
    --expect "trace: traced 1.5 42 NaN" --expect "after trace"
# A host fault in a nested script call also ends the call it nests in.
node run.mjs $HARNESS fixture/nested_fault.riv --fault raise \
    --expect "wasm call trapped in host_data_value_changed: librive faulted in a host call" \
    --expect "wasm call trapped in host_obj_user_init: librive faulted in a host call, so its script module stops: librive faulted in a nested script call" \
    --expect "script vms alive after the frames: 0"
# A JS error thrown under librive frames is a host fault: unlike a script
# trap, the module stops for good.
node run.mjs $HARNESS fixture/trap.riv --fault read \
    --expect "wasm call trapped in host_obj_user_init: librive faulted in a host call" \
    --expect "last trap: librive faulted in a host call" \
    --expect "script vms alive after the frames: 0"
# A SIMD module runs where the engine has SIMD. Where it lacks the flavor the
# module was baked with, the file's scripts are off with one message saying
# why, and the artboard still draws.
node run.mjs $HARNESS fixture/simd.riv --expect "simd lane 22"
node run.mjs $HARNESS fixture/relaxed_simd.riv --expect "relaxed lane 7"
node run.mjs $HARNESS fixture/scalar.riv --expect "scalar lane 3"
for MODE in "" --sync; do
    node run.mjs $HARNESS fixture/simd.riv $MODE --lacks simd --vms 0 \
        --expect "this browser lacks WebAssembly SIMD, so this file's scripts are off; bake with wasmSimd: off to support it" \
        --expect "artboard drew 5 frames"
    node run.mjs $HARNESS fixture/relaxed_simd.riv $MODE --lacks relaxed --vms 0 \
        --expect "this browser lacks WebAssembly relaxed SIMD, so this file's scripts are off; bake with wasmSimd: on to support it" \
        --expect "artboard drew 5 frames"
    # A module refused for a feature other than the SIMD it uses carries the
    # engine's own reason.
    node run.mjs $HARNESS fixture/scalar.riv $MODE --lacks simd,bulk --vms 0 \
        --expect "module instantiate failed: this engine lacks bulk memory" \
        --expect "artboard drew 5 frames"
    node run.mjs $HARNESS fixture/simd.riv $MODE --lacks relaxed,bulk --vms 0 \
        --expect "module instantiate failed: this engine lacks bulk memory" \
        --expect "artboard drew 5 frames"
done
# An engine validates only the main module, so relaxed SIMD that only a job
# or a processor uses turns the file's scripts off the same way. Plain SIMD
# always reaches the main module too.
node run.mjs $HARNESS fixture/simd_job.riv --expect "job relaxed lane 7"
node run.mjs $HARNESS fixture/simd_processor.riv --audio 1 \
    --until "processor relaxed lane" --expect "processor relaxed lane 7"
for MODE in "" --sync; do
    for FIXTURE in simd_job simd_processor; do
        node run.mjs $HARNESS fixture/$FIXTURE.riv $MODE --lacks relaxed --vms 0 \
            --expect "this browser lacks WebAssembly relaxed SIMD, so this file's scripts are off; bake with wasmSimd: on to support it" \
            --expect "artboard drew 5 frames"
    done
done

# Browser pages, in Chrome when it is installed, CHROME naming another binary,
# and in firefox and webkit when RIVE_WEB_BROWSERS names them.
CHROME="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
BROWSERS=($RIVE_WEB_BROWSERS)
if [[ -x "$CHROME" ]]; then
    BROWSERS=(chrome "${BROWSERS[@]}")
else
    echo "skipped Chrome's browser lanes: no Chrome at $CHROME"
fi
# Firefox can leave a page's audio context suspended after its click, so a
# failed page outside Chrome runs once more, with a warning on GitHub.
page() {
    CHROME="$CHROME" node browser_audio.mjs "$@" --browser $BROWSER && return
    if [[ $BROWSER == chrome ]]; then
        return 1
    fi
    echo "::warning::$2 ($1 $3) failed once in $BROWSER and runs again"
    CHROME="$CHROME" node browser_audio.mjs "$@" --browser $BROWSER
}
for BROWSER in "${BROWSERS[@]}"; do
    # Script audio in real time on the page's audio worklet, which runs each
    # processor's own module and reaches librive's heap through Atomics.
    PAGE=out/wasm_threads_debug
    RAMP=(--expect "ramp init 2 channels" --expect "processor running"
        --expect "processor threw bad block" --expect "processor meter")
    page $PAGE fixture/audio.riv $STEADY "${RAMP[@]}"
    # Given no context the runner makes its own, which Chrome's autoplay flag
    # starts at once. Elsewhere it warns, then starts on the page's click.
    WAITS=()
    if [[ $BROWSER != chrome ]]; then
        WAITS=(--expect "warning: script audio waits for a click")
    fi
    page $PAGE fixture/audio.riv --runner-context $STEADY "${WAITS[@]}" "${RAMP[@]}"
    page $PAGE fixture/feed.riv $STEADY --expect "feed received 4096 sum 8386560.0"
    page $PAGE fixture/processor_holds.riv $STEADY "${HOLDS[@]:3}"
    page $PAGE fixture/blob.riv $STEADY --expect "sampler played 4096 sum 4193280.0"
    # A browser worker's own clock starts after the page's, unlike node's.
    page $PAGE fixture/lane_clock.riv \
        --expect "lane clock job agrees yes" --expect "lane clock worker agrees yes"
    # A processor's null string does not end it on a real worklet either.
    page $PAGE fixture/lane_console_null.riv $STEADY \
        --expect "processor after the null log"
    # A block past its budget ends at its next fuel check, or without fuel
    # once it returns, failing as the native watchdog does.
    for FIXTURE in overrun overrun_release; do
        page $PAGE fixture/$FIXTURE.riv \
            --expect "slow failed: processor overran its block budget"
    done
    # A metered block stuck past its budget ends there and the mix plays on,
    # as do inits past their own budget, each of which the page waits out.
    for FIXTURE in worklet_stall worklet_stall_fuel; do
        page $PAGE fixture/$FIXTURE.riv --hard-blocks 20 \
            --expect "stuck failed: processor overran its block budget" \
            --expect "quiet rendered after stuck failed"
    done
    page $PAGE fixture/init_spin.riv $STEADY "${INIT_SPIN[@]:2}"
    # Nothing can end a worklet stuck in a block without fuel, so the page
    # fails every processor it runs once its blocks stop.
    page $PAGE fixture/worklet_stall_release.riv $STEADY \
        --expect "stuck failed: processor stalled the audio thread" \
        --expect "quiet failed: processor stalled the audio thread"
    # The browser's own worklet renders the samples harness_audio hashes,
    # with SIMD or without.
    for FIXTURE in "${DETERMINISM[@]}" determinism_scalar; do
        page $PAGE fixture/$FIXTURE.riv $STEADY --hash 16 \
            --expect "worklet rendered 16 blocks $SAME_SAMPLES"
    done
    # A page that names no worklet and serves none of its files renders the
    # same samples on the one the threaded build carries.
    page $PAGE fixture/determinism.riv $STEADY --hash 16 --carried-worklet \
        --expect "worklet rendered 16 blocks $SAME_SAMPLES"
    # Jobs on browser workers, and a page thread that cannot sleep spinning
    # while it waits on a worker.
    page $PAGE fixture/jobs.riv "${JOBS[@]:2}"
    page $PAGE fixture/job_spin.riv "${SPINNERS[@]:2}"
    page $PAGE fixture/ports.riv "${PORTS[@]}" \
        --expect "port answered within the call 7.0"
    page $PAGE fixture/parallel.riv "${TEAM[@]}" "${PARALLEL[@]}" \
        --expect "parallel parts all yes"
    page $PAGE fixture/parallel_stop.riv --parallel-parts 4 "${STOPPED[@]}"
    page $PAGE fixture/parallel_design.riv --parallel-parts 4 "${DESIGN[@]}"
    # Calls on every frame, each frame's first waking a parked team. WebKit
    # can lose the wake of an async wait, which hung the page, so its team
    # waits with a timeout. Without audio a failure runs only once.
    WAKES=async
    if [[ $BROWSER == webkit ]]; then
        WAKES=timed
    fi
    for PARTS in 4 8; do
        CHROME="$CHROME" node browser_audio.mjs $PAGE fixture/parallel_frames.riv \
            --browser $BROWSER --parallel-parts $PARTS \
            --expect "frames 600 parts $PARTS wrong 0" \
            --expect "page parallel {\"team\":$((PARTS - 1)),\"waits\":\"$WAKES\"}"
    done
    page $PAGE fixture/basic.riv --no-isolation --expect "$REFUSED"
    # A page without cross origin isolation loads the build without threads,
    # which runs lanes on the page's thread and refuses real time audio.
    PAGE=out/wasm_debug
    page $PAGE fixture/jobs.riv --no-isolation "${JOBS[@]}"
    page $PAGE fixture/workers.riv --no-isolation "${WORKERS[@]}"
    page $PAGE fixture/parallel.riv --no-isolation "${SERIAL[@]}" "${PARALLEL[@]}"
    page $PAGE fixture/parallel_stop.riv --no-isolation "${STOPPED[@]}"
    page $PAGE fixture/parallel_design.riv --no-isolation "${DESIGN[@]}"
    page $PAGE fixture/job_spin.riv --no-isolation \
        --expect "spinner failed: $SPINNER_TIMEOUT" \
        --expect "job after the spinners 9.0"
    page $PAGE fixture/worker_spin.riv --no-isolation \
        --expect "worker spinner failed: $SPINNER_TIMEOUT"
    for FIXTURE in "${DETERMINISM[@]}"; do
        page $PAGE fixture/$FIXTURE.riv --no-isolation --offline 16 \
            --expect "processor 0 state 1 $SAME_SAMPLES"
    done
    page $PAGE fixture/audio.riv --no-isolation \
        --expect "processor threw real time audio needs cross origin isolation"
done
