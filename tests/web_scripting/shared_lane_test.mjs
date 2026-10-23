// Checks rive_shared_lane.mjs on a heap laid out by hand from
// rive_shared_layout.mjs, so it runs without librive: lookups against stale
// and retired ids, blob slices, snapshot readers racing publishers on other
// threads, sequences carrying into their high word, a lane past the table's
// lane words, and a page freeing what a lane it ended still held.
//   node shared_lane_test.mjs
import { Worker, isMainThread, parentPort, workerData } from 'worker_threads';
import { fileURLToPath } from 'url';
import {
    kBlobData,
    kChunkSlots,
    kHeaderCapacity,
    kHeaderKind,
    kHeaderLock,
    kHoldLock,
    kHoldToken,
    kHoldWords,
    kIdBit,
    kIdSlotShift,
    kKindBlob,
    kKindRegion,
    kKindRing,
    kKindSignal,
    kKindSnapshot,
    kLaneCount,
    kLaneFirstToken,
    kLaneIdle,
    kRecordHolds,
    kRegionData,
    kRingData,
    kRingUsed,
    kSignalSeq,
    kSignalSeqHigh,
    kRingWriteFull,
    kRingWriteNeverFits,
    kRingWriteWritten,
    kSignalWatcherCount,
    kSignalWatchers,
    kSlotBlock,
    kSlotLive,
    kSlotOwner,
    kSlotOwnerHigh,
    kSlotSize,
    kSlotState,
    kSnapshotData,
    kSnapshotCopies,
    kSnapshotLatest,
    kSnapshotSizes,
    kSnapshotSeq,
    kSnapshotSeqHigh,
    kTableChunks,
    kTableLanes,
    kTableTokens,
} from '../../src/wasm/web/rive_shared_layout.mjs';
import { kProcessorHolds } from '../../src/wasm/web/rive_lane_imports.mjs';
import { createSharedLaneImports, releaseSharedLane } from '../../src/wasm/web/rive_shared_lane.mjs';

const kTable = 1024;
const kChunk = 8192;
const kBlocks = kChunk + kChunkSlots * kSlotSize;
// Past 32 bits, so both owner words count.
const kOwner = 2 ** 32 + 5;

// A lane over heap, with a module memory of moduleBytes for its arguments.
function laneOver(heap, moduleBytes, holds = null, fail = undefined, table = kTable) {
    const memory = { buffer: new ArrayBuffer(moduleBytes) };
    const imports = createSharedLaneImports(() => heap, table, kOwner, () => memory, holds, fail);
    return { imports, bytes: new Uint8Array(memory.buffer) };
}

// A lane claims its word once per table, so a check of a fresh claim takes
// a table of its own.
function makeHeap(bytes, table = kTable) {
    const heap = new SharedArrayBuffer(bytes);
    new Uint32Array(heap)[(table + kTableChunks) >>> 2] = kChunk;
    return { heap, u32: new Uint32Array(heap), next: kBlocks, slots: 0 };
}

// Adds a block as SharedRegistry::add would and answers its id.
function add(h, kind, blockSize, capacity, dataSize, owner = kOwner) {
    const block = h.next;
    h.next += (blockSize + dataSize + 7) & ~7;
    h.u32[(block + kHeaderKind) >>> 2] = kind;
    h.u32[(block + kHeaderCapacity) >>> 2] = capacity;
    const index = h.slots++;
    const slot = kChunk + index * kSlotSize;
    h.u32[(slot + kSlotBlock) >>> 2] = block;
    h.u32[(slot + kSlotOwner) >>> 2] = owner % 2 ** 32;
    h.u32[(slot + kSlotOwnerHigh) >>> 2] = Math.floor(owner / 2 ** 32);
    Atomics.store(h.u32, (slot + kSlotState) >>> 2, kSlotLive);
    return { id: (kIdBit | (index << kIdSlotShift)) >>> 0, block, slot };
}

let failures = 0;
function check(ok, what) {
    if (!ok) {
        failures++;
        console.log('FAIL ' + what);
    }
}

async function main() {
    const self = fileURLToPath(import.meta.url);

    {
        const h = makeHeap(1 << 20);
        const holds = new Int32Array(new SharedArrayBuffer(kHoldWords * 4));
        const { imports, bytes } = laneOver(h.heap, 4096, holds);
        check(holds[kHoldToken] === kLaneFirstToken, 'the first lane claims the first token');
        check(h.u32[(kTable + kTableLanes) >>> 2] === kLaneIdle, 'a claimed lane word is idle');
        const ring = add(h, kKindRing, kRingData, 64, 64);
        bytes.set([5, 6, 7], 16);
        check(imports['ring_write'](ring.id, 16, 3, 0, 0) === kRingWriteWritten, 'a ring takes a record');
        check(imports['ring_used'](ring.id) === 7, 'a record counts its length');
        check(imports['ring_read'](ring.id, 64, 8) === 4, 'a read answers the size plus one');
        check(bytes[66] === 7, 'a read copies the record out');
        check(h.u32[(kTable + kTableLanes) >>> 2] === kLaneIdle, 'a call leaves nothing pinned');
        check(h.u32[(ring.block + kHeaderLock) >>> 2] === 0, 'a call leaves nothing locked');
        check(imports['ring_write'](ring.id, 16, 61, 0, 0) === kRingWriteNeverFits,
            'a record larger than the ring never fits');
        for (let i = 0; i < 6; i++) {
            imports['ring_write'](ring.id, 16, 6, 0, 0);
        }
        check(imports['ring_write'](ring.id, 16, 6, 0, 0) === kRingWriteFull, 'a full ring refuses for now');
        check(imports['ring_drop'](ring.id, 6) === 0, 'a drop leaves a record that fits');
        check(imports['ring_drop'](ring.id, 4) === 7, 'a drop pops a record too large');
        check(imports['ring_used'](ring.id) === 50, 'a dropped record frees its bytes');
        imports['ring_drop'](ring.id, 0);
        check(imports['ring_read'](ring.id, 64, 8) === 7, 'the record after a drop still reads');
        while (imports['ring_drop'](ring.id, 0) !== 0) {}
        check(imports['ring_used'](ring.id) === 0, 'drops empty the ring');
        check(h.u32[(ring.block + kHeaderLock) >>> 2] === 0, 'a drop leaves nothing locked');
        // A stale generation, a retired slot and another kind find nothing.
        check(imports['ring_used']((ring.id + 1) >>> 0) === 0, 'a stale id finds nothing');
        check(imports['snapshot_seq'](ring.id) === 0, 'an id of another kind finds nothing');
        Atomics.and(h.u32, (ring.slot + kSlotState) >>> 2, ~kSlotLive);
        check(imports['ring_used'](ring.id) === 0, 'a retired slot finds nothing');
        const foreign = add(h, kKindRing, kRingData, 64, 64, kOwner + 2 ** 32);
        check(imports['ring_write'](foreign.id, 16, 3, 0, 0) === kRingWriteFull, "another VM's ring takes nothing");
        check(imports['ring_used'](foreign.id) === 0, "another VM's ring finds nothing");
        check(h.u32[(kTable + kTableLanes) >>> 2] === kLaneIdle, 'a refused lookup leaves nothing pinned');
    }

    {
        // Atomics answer -1 outside the region and once it is gone.
        const h = makeHeap(1 << 20);
        const { imports } = laneOver(h.heap, 64);
        const region = add(h, kKindRegion, kRegionData, 8, 8);
        check(imports['atomic_store'](region.id, 4, 7) === 0, 'a store in the region answers 0');
        check(imports['atomic_add'](region.id, 4, 1) === 7, 'an add answers the value before it');
        check(imports['atomic_cas'](region.id, 4, 8, 0xffffffff) === 8, 'a compare exchange answers the old value');
        check(imports['atomic_load'](region.id, 4) === 0xffffffff, 'a load answers the word unsigned');
        check(imports['atomic_load'](region.id, 8) === -1, 'a word past the region answers -1');
        check(imports['atomic_store'](region.id, 2, 1) === -1, 'an unaligned word answers -1');
        Atomics.and(h.u32, (region.slot + kSlotState) >>> 2, ~kSlotLive);
        check(imports['atomic_cas'](region.id, 0, 0, 1) === -1, 'a gone region answers -1');
    }

    {
        // As a processor's natives, the lane leaves records and values that
        // name shared objects in place and fails, and refuses to send any.
        const h = makeHeap(1 << 20);
        const failures = [];
        const { imports, bytes } = laneOver(h.heap, 4096, null, (text) => failures.push(text));
        const refused = (what) => {
            check(failures.length === 1 && failures[0] === kProcessorHolds, what + ' fails the lane');
            failures.length = 0;
        };
        const ring = add(h, kKindRing, kRingData, 64, 64);
        const record = new DataView(h.heap, ring.block + kRingData, 20);
        record.setUint32(0, (3 + 8) | kRecordHolds, true);
        new Uint8Array(h.heap).set([5, 6, 7], ring.block + kRingData + 4);
        record.setUint32(7, 0x80001000, true);
        record.setUint32(11, 1, true);
        h.u32[(ring.block + kRingUsed) >>> 2] = 15;
        check(imports['ring_read'](ring.id, 64, 8) === 0, 'a read of a held record answers nothing');
        refused('a read of a held record');
        check(imports['ring_read'](ring.id, 0, 0) === 0, 'a size query of a held record answers nothing');
        refused('a size query of a held record');
        check(imports['ring_used'](ring.id) === 15, 'a held record stays in the ring');
        check(h.u32[(ring.block + kHeaderLock) >>> 2] === 0, 'a refused read leaves nothing locked');
        h.u32[(ring.block + kRingUsed) >>> 2] = 0;
        bytes.set([1, 2], 64);
        check(imports['ring_write'](ring.id, 64, 2, 128, 1) === 0, 'a record naming shared objects is not sent');
        refused('a send naming shared objects');
        check(imports['ring_used'](ring.id) === 0, 'a refused send writes nothing');
        check(imports['ring_write'](ring.id, 64, 2, 0, 0) === 1 && failures.length === 0, 'a plain record still sends');

        const snapshot = add(h, kKindSnapshot, kSnapshotData, 32, 32 * kSnapshotCopies);
        const value = new DataView(h.heap, snapshot.block + kSnapshotData, 32);
        value.setUint32(0, 42, true);
        value.setUint32(4, 0x80001000, true);
        value.setUint32(8, 1, true);
        h.u32[(snapshot.block + kSnapshotSizes) >>> 2] = (4 + 8) | kRecordHolds;
        h.u32[(snapshot.block + kSnapshotLatest) >>> 2] = 1;
        new DataView(bytes.buffer).setUint32(64, 7, true);
        check(imports['snapshot_read'](snapshot.id, 64, 8) === 0, 'a read of a held value answers nothing');
        refused('a read of a held value');
        check(new DataView(bytes.buffer).getUint32(64, true) === 7, 'a refused read copies nothing');
        check(imports['snapshot_publish'](snapshot.id, 64, 4, 0, 0) === 0, 'a publish over a held value is refused');
        refused('a publish over a held value');
        check(h.u32[(snapshot.block + kSnapshotLatest) >>> 2] === 1, 'a refused publish keeps the held value');
        check(h.u32[(snapshot.block + kHeaderLock) >>> 2] === 0, 'a refused publish leaves nothing locked');
        h.u32[(snapshot.block + kSnapshotSizes) >>> 2] = 4;
        check(imports['snapshot_publish'](snapshot.id, 64, 4, 128, 1) === 0, 'a value naming shared objects is not published');
        refused('a publish naming shared objects');
        check(imports['snapshot_publish'](snapshot.id, 64, 40, 0, 0) === 0 && failures.length === 0,
            'a value too large is refused without failing');
        check(imports['snapshot_publish'](snapshot.id, 64, 4, 0, 0) === 1, 'a publish over a plain value lands');
        check(imports['snapshot_read'](snapshot.id, 68, 8) === 5 && new DataView(bytes.buffer).getUint32(68, true) === 7,
            'a reader sees the new value');
        check(failures.length === 0, 'plain values never fail the lane');
    }

    {
        const h = makeHeap(1 << 20);
        const { imports, bytes } = laneOver(h.heap, 64);
        const blob = add(h, kKindBlob, kBlobData, 10, 10);
        new Uint8Array(h.heap).set([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], blob.block + kBlobData);
        check(imports['blob_size'](blob.id) === 10, 'a blob answers its size');
        check(imports['blob_read'](blob.id, 2, 16, 4) === 4, 'a read fills its buffer');
        check(bytes[16] === 2 && bytes[19] === 5, 'a read copies from its offset');
        check(imports['blob_read'](blob.id, 8, 16, 4) === 2, 'a read near the end copies what is left');
        check(bytes[17] === 9, 'the last bytes land first in the buffer');
        check(imports['blob_read'](blob.id, 10, 16, 4) === 0, 'a read past the end copies nothing');
        check(imports['blob_read'](blob.id, 0, 62, 4) === 0, 'a buffer outside the lane copies nothing');
        check(imports['blob_size'](imports['blob_create'](16, 4)) === 0, 'a lane cannot create a blob');
        check(imports['ring_used'](blob.id) === 0, 'a blob is no ring');
        check(h.u32[(kTable + kTableLanes) >>> 2] === kLaneIdle, 'a blob read leaves nothing pinned');
    }

    {
        // Readers on their own threads never see a mix of two publishes.
        const h = makeHeap(1 << 20);
        const words = 64;
        const snapshot = add(
            h,
            kKindSnapshot,
            kSnapshotData,
            words * 4,
            words * 4 * kSnapshotCopies
        );
        // Words: lanes ready, publishers done.
        const control = new Int32Array(new SharedArrayBuffer(8));
        const roles = ['publish', 'publish', 'read', 'read'];
        const lanes = roles.length;
        const publishers = roles.filter((role) => role === 'publish').length;
        const spawn = (role, index) =>
            new Promise((done) => {
                const worker = new Worker(self, {
                    workerData: { role, index, heap: h.heap, id: snapshot.id, words, control, lanes, publishers },
                });
                worker.on('message', done);
            });
        const results = await Promise.all(roles.map((role, i) => spawn(role, i % 2)));
        const reads = results.filter((r) => r.role === 'read');
        check(reads.every((r) => r.torn === 0), 'no reader saw a torn value');
        check(reads.every((r) => r.reads > 0), 'every reader saw values');
        const published = results.reduce((sum, r) => sum + (r.published ?? 0), 0);
        const low = h.u32[(snapshot.block + kSnapshotSeq) >>> 2];
        const high = h.u32[(snapshot.block + kSnapshotSeqHigh) >>> 2];
        check(high === 0 && low === published, 'every publish counts');
    }

    {
        // A sequence carries into its high word, and reads as one number.
        const h = makeHeap(1 << 20);
        const { imports } = laneOver(h.heap, 64);
        const signal = add(h, kKindSignal, 64, 0, 0);
        h.u32[(signal.block + kSignalSeq) >>> 2] = 0xffffffff;
        h.u32[(signal.block + kSignalSeqHigh) >>> 2] = 2;
        check(imports['signal_seq'](signal.id) === 3 * 2 ** 32 - 1, 'a sequence reads both words');
        imports['signal_notify'](signal.id);
        check(h.u32[(signal.block + kSignalSeq) >>> 2] === 0, 'a notify wraps the low word');
        check(imports['signal_seq'](signal.id) === 3 * 2 ** 32, 'and carries into the high word');
    }

    {
        // Past the lane words a lane still takes a token of its own, whose lock
        // the page frees, but no lane word.
        const table = 4096;
        const h = makeHeap(1 << 20, table);
        const lanes = (table + kTableLanes) >>> 2;
        h.u32.fill(kLaneIdle, lanes, lanes + kLaneCount);
        const holds = new Int32Array(new SharedArrayBuffer(kHoldWords * 4));
        laneOver(h.heap, 64, holds, undefined, table);
        const token = Atomics.load(holds, kHoldToken);
        check(token === kLaneFirstToken + kLaneCount, 'a lane past the lane words takes the next token');
        check(h.u32[(table + kTableTokens) >>> 2] === 1, 'the table counts that token');
        const ring = add(h, kKindRing, kRingData, 64, 64);
        const lockAt = (ring.block + kHeaderLock) >>> 2;
        Atomics.store(h.u32, lockAt, token);
        Atomics.store(holds, kHoldLock, ring.block + kHeaderLock);
        releaseSharedLane(() => h.heap, table, holds);
        check(h.u32[lockAt] === 0, 'the page frees the lock of a lane past the lane words');
        check(h.u32.subarray(lanes, lanes + kLaneCount).every((word) => word === kLaneIdle),
            'and leaves every lane word alone');
    }

    {
        // A lane ended while it notifies a signal leaves the lock and the slot
        // pin behind, which the page frees and nothing else.
        const watchers = 8 << 20;
        const h = makeHeap(watchers * 4 + (1 << 20));
        const signal = add(h, kKindSignal, 64, 0, 8 + watchers * 4);
        const wake = signal.block + 64;
        const list = wake + 8;
        h.u32[(signal.block + kSignalWatcherCount) >>> 2] = watchers;
        h.u32[(signal.block + kSignalWatchers) >>> 2] = list;
        h.u32.fill(wake, list >>> 2, (list >>> 2) + watchers);
        const holds = new Int32Array(new SharedArrayBuffer(kHoldWords * 4));
        const worker = new Worker(self, {
            workerData: { role: 'stall', heap: h.heap, id: signal.id, holds },
        });
        const lockAt = (signal.block + kHeaderLock) >>> 2;
        const i32 = new Int32Array(h.heap);
        while (Atomics.load(i32, lockAt) === 0) {}
        await worker.terminate();
        const token = Atomics.load(holds, kHoldToken);
        check(token >= kLaneFirstToken, 'the lane records its token');
        check(Atomics.load(i32, lockAt) === token, 'the ended lane still holds the lock');
        const laneAt = ((kTable + kTableLanes) >>> 2) + token - kLaneFirstToken;
        check(Atomics.load(i32, laneAt) === signal.slot, 'the ended lane still pins the slot');
        check(Atomics.load(holds, kHoldLock) === signal.block + kHeaderLock, 'the lane records its lock');
        releaseSharedLane(() => h.heap, kTable, holds);
        check(Atomics.load(i32, lockAt) === 0, 'the page frees the lock');
        check(Atomics.load(i32, laneAt) === 0, 'the page frees the lane word');
        // A lock someone else took since is left alone.
        Atomics.store(i32, lockAt, 1);
        releaseSharedLane(() => h.heap, kTable, holds);
        check(Atomics.load(i32, lockAt) === 1, 'the page keeps a lock it did not take');
    }

    console.log(failures === 0 ? 'shared lane ok' : failures + ' shared lane checks failed');
    process.exit(failures === 0 ? 0 : 1);
}

function worker() {
    const { role, heap } = workerData;
    if (role === 'stall') {
        laneOver(heap, 64, workerData.holds).imports['signal_notify'](workerData.id);
        return;
    }
    const { words, control, lanes, publishers } = workerData;
    // Address 0 means no buffer to the natives.
    const at = 64;
    const { imports, bytes } = laneOver(heap, at + words * 4);
    const values = new Uint32Array(bytes.buffer, at, words);
    // Every lane starts together, so reads race publishes however slowly
    // the threads come up.
    Atomics.add(control, 0, 1);
    while (Atomics.load(control, 0) < lanes) {}
    if (role === 'publish') {
        let published = 0;
        while (published < 20000) {
            published++;
            const word = published * 2 + workerData.index;
            const count = 1 + (word % words);
            values.fill(word, 0, count);
            imports['snapshot_publish'](workerData.id, at, count * 4, 0, 0);
        }
        Atomics.add(control, 1, 1);
        parentPort.postMessage({ role, published });
        return;
    }
    let torn = 0;
    let reads = 0;
    // Past the last publish a read always finds a value.
    while (Atomics.load(control, 1) < publishers || reads === 0) {
        const size = imports['snapshot_read'](workerData.id, at, words * 4);
        if (size === 0) {
            continue;
        }
        reads++;
        const count = (size - 1) / 4;
        let whole = count === 1 + (values[0] % words);
        for (let k = 0; k < count; k++) {
            whole = whole && values[k] === values[0];
        }
        torn += whole ? 0 : 1;
    }
    parentPort.postMessage({ role, torn, reads });
}

if (isMainThread) {
    await main();
} else {
    worker();
}
