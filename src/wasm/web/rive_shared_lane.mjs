// The rive_shared_v1 natives for a lane without librive, such as an audio
// worklet, through Atomics on librive's heap; like librive's, none ever waits.

import {
    kBlobData,
    kChunkShift,
    kChunkSlots,
    kHeaderCapacity,
    kHeaderKind,
    kHeaderLock,
    kHoldLock,
    kHoldToken,
    kIdBit,
    kIdGenerationMask,
    kIdSlotShift,
    kKindBlob,
    kKindRegion,
    kKindRing,
    kKindSignal,
    kKindSnapshot,
    kLaneCount,
    kLaneFirstToken,
    kLaneIdle,
    kRegionData,
    kRegionLockGone,
    kRecordHolds,
    kRegionPoisoned,
    kRingData,
    kRingDataSignal,
    kRingHead,
    kRingSpaceSignal,
    kRingUsed,
    kRingWriteFull,
    kRingWriteNeverFits,
    kRingWriteWritten,
    kSignalSeq,
    kSignalSeqHigh,
    kSignalWatcherCount,
    kSignalWatchers,
    kSlotBlock,
    kSlotLive,
    kSlotOwner,
    kSlotOwnerHigh,
    kSlotPin,
    kSlotSize,
    kSlotState,
    kSnapshotCopies,
    kSnapshotData,
    kSnapshotLatest,
    kSnapshotSeq,
    kSnapshotSeqHigh,
    kSnapshotSizes,
    kSnapshotVersions,
    kTableChunks,
    kTableLanes,
    kTableTokens,
    kWakeWordWord,
} from './rive_shared_layout.mjs';
import { kProcessorHolds } from './rive_lane_imports.mjs';

// Traps as WAMR does when byteLength bytes at ptr leave a module's memory.
function checkBounds(buffer, ptr, byteLength) {
    if (ptr + byteLength > buffer.byteLength) {
        throw new Error('out of bounds memory access');
    }
}

// The byte length of the std string at ptr, which the word before it holds.
// Null reads as empty. Addresses and lengths arrive from wasm as signed i32s.
export function managedLength(buffer, ptr) {
    ptr >>>= 0;
    if (ptr < 4) {
        return 0;
    }
    checkBounds(buffer, ptr, 0);
    const length = new DataView(buffer).getUint32(ptr - 4, true);
    checkBounds(buffer, ptr, length);
    return length;
}

// UTF-16 text in a module's memory. Audio worklet scopes may have no
// TextDecoder.
export function utf16Text(buffer, ptr, byteLength) {
    ptr >>>= 0;
    byteLength >>>= 0;
    checkBounds(buffer, ptr, byteLength);
    if (typeof TextDecoder !== 'undefined') {
        return new TextDecoder('utf-16le', { ignoreBOM: true }).decode(
            new Uint8Array(buffer, ptr, byteLength)
        );
    }
    // A Uint16Array cannot start at an odd address, so text there is copied.
    const units =
        ptr % 2 === 0
            ? new Uint16Array(buffer, ptr, byteLength >> 1)
            : new Uint16Array(buffer.slice(ptr, ptr + byteLength - (byteLength % 2)));
    let text = '';
    for (let i = 0; i < units.length; i += 4096) {
        text += String.fromCharCode.apply(null, units.subarray(i, i + 4096));
    }
    return text;
}

// Each thread's lane word and lock token, per table. Past the table's lane
// words a lane pins through the slot's count, which no page can free.
const laneClaims = new Map();
const kOverflowToken = kLaneFirstToken + kLaneCount;

function claimLane(i32, table) {
    let claim = laneClaims.get(table);
    if (claim === undefined) {
        for (let i = 0; i < kLaneCount && claim === undefined; i++) {
            const word = (table + kTableLanes + i * 4) >>> 2;
            if (Atomics.compareExchange(i32, word, 0, kLaneIdle) === 0) {
                claim = { word, token: kLaneFirstToken + i };
            }
        }
        // Still a token of its own, so its locks tell from librive's and
        // other lanes'.
        claim ??= {
            word: -1,
            token: kOverflowToken + Atomics.add(i32, (table + kTableTokens) >>> 2, 1),
        };
        laneClaims.set(table, claim);
    }
    return claim;
}

// The id names the slot's live object.
const slotNames = (state, id) =>
    (state & kSlotLive) !== 0 && ((state ^ id) & kIdGenerationMask) === 0;

// heap() answers librive's heap buffer, table is SharedRegistry::table() and
// owner the VM whose objects the lane reaches; the lane records its token and
// lock in holds, for a page to free them.
export function createSharedLaneImports(heap, table, owner, moduleMemory, holds = null, fail = () => {}) {
    table >>>= 0;
    const ownerLow = owner % 0x100000000;
    const ownerHigh = Math.floor(owner / 0x100000000);
    // Views over the heap and the lane's memory, rebuilt only when growth
    // replaces a buffer, so no call allocates on the audio thread.
    let buffer = null;
    let u8 = null;
    let i32 = null;
    let u32 = null;
    let moduleBuffer = null;
    let moduleU8 = null;
    const views = () => {
        const current = heap();
        if (current !== buffer) {
            buffer = current;
            u8 = new Uint8Array(buffer);
            i32 = new Int32Array(buffer);
            u32 = new Uint32Array(buffer);
        }
    };
    views();
    const claim = claimLane(i32, table);
    if (holds !== null) {
        Atomics.store(holds, kHoldToken, claim.token);
    }
    // The lane's bytes when [ptr, ptr + length) lies in its memory, checked
    // before any lock is taken; null otherwise, as WAMR would trap.
    const moduleRange = (ptr, length) => {
        const current = moduleMemory().buffer;
        if (current !== moduleBuffer) {
            moduleBuffer = current;
            moduleU8 = new Uint8Array(current);
        }
        return ptr + length <= moduleU8.length ? moduleU8 : null;
    };
    const copy = (to, toAt, from, fromAt, size) => {
        for (let i = 0; i < size; i++) {
            to[toAt + i] = from[fromAt + i];
        }
    };
    // Recorded before the swap, so whichever way an ended thread stopped,
    // the word holds this token only if the lane took it.
    const lock = (address) => {
        if (holds !== null) {
            Atomics.store(holds, kHoldLock, address);
        }
        while (Atomics.compareExchange(i32, address >>> 2, 0, claim.token) !== 0) {}
    };
    const unlock = (address) => {
        Atomics.store(i32, address >>> 2, 0);
        if (holds !== null) {
            Atomics.store(holds, kHoldLock, 0);
        }
    };

    // The block an id names, pinned so librive cannot free it until unpin;
    // 0 when the id is stale, another VM's or of another kind. A lane pins
    // one at a time.
    let pinned = 0;
    const pin = (id, kind) => {
        views();
        id >>>= 0;
        if ((id & kIdBit) === 0) {
            return 0;
        }
        const index = (id & ~kIdBit) >>> kIdSlotShift;
        const chunk = Atomics.load(u32, (table + kTableChunks + (index >>> kChunkShift) * 4) >>> 2);
        if (chunk === 0) {
            return 0;
        }
        const slot = chunk + (index & (kChunkSlots - 1)) * kSlotSize;
        const stateAt = (slot + kSlotState) >>> 2;
        if (claim.word >= 0) {
            // Stored before the state is checked, as librive retires a slot
            // before scanning the lane words: one of the two sees the other.
            Atomics.store(u32, claim.word, slot);
            if (!slotNames(Atomics.load(u32, stateAt), id)) {
                Atomics.store(u32, claim.word, kLaneIdle);
                return 0;
            }
        } else {
            for (;;) {
                const state = Atomics.load(u32, stateAt);
                if (!slotNames(state, id)) {
                    return 0;
                }
                if (Atomics.compareExchange(u32, stateAt, state, state + kSlotPin) === state) {
                    break;
                }
            }
        }
        pinned = slot;
        const block = Atomics.load(u32, (slot + kSlotBlock) >>> 2);
        if (
            u32[(block + kHeaderKind) >>> 2] !== kind ||
            u32[(slot + kSlotOwner) >>> 2] !== ownerLow ||
            u32[(slot + kSlotOwnerHigh) >>> 2] !== ownerHigh
        ) {
            unpin();
            return 0;
        }
        return block;
    };
    const unpin = () => {
        if (claim.word >= 0) {
            Atomics.store(u32, claim.word, kLaneIdle);
        } else {
            Atomics.sub(u32, (pinned + kSlotState) >>> 2, kSlotPin);
        }
    };
    const capacityOf = (block) => u32[(block + kHeaderCapacity) >>> 2];
    // A sequence is a low and a high word, as a 64 bit atomic allocates here.
    const bumpSeq = (low, high) => {
        if (Atomics.add(u32, low >>> 2, 1) === 0xffffffff) {
            Atomics.add(u32, high >>> 2, 1);
        }
    };
    // High first, so a read during a carry answers less, never more.
    const readSeq = (low, high) => {
        const top = Atomics.load(u32, high >>> 2);
        return top * 0x100000000 + Atomics.load(u32, low >>> 2);
    };

    const notify = (signal) => {
        const block = pin(signal, kKindSignal);
        if (block === 0) {
            return;
        }
        bumpSeq(block + kSignalSeq, block + kSignalSeqHigh);
        lock(block + kHeaderLock);
        try {
            const count = u32[(block + kSignalWatcherCount) >>> 2];
            const watchers = u32[(block + kSignalWatchers) >>> 2] >>> 2;
            for (let i = 0; i < count; i++) {
                // A watcher's wake word: a futex word librive's threads wait on.
                const word = (u32[watchers + i] + kWakeWordWord) >>> 2;
                Atomics.add(i32, word, 1);
                Atomics.notify(i32, word);
            }
        } finally {
            unlock(block + kHeaderLock);
            unpin();
        }
    };

    // Ring bytes wrap around the block's data.
    const ringCopyIn = (block, at, from, fromAt, size) => {
        const capacity = capacityOf(block);
        const first = Math.min(size, capacity - at);
        copy(u8, block + kRingData + at, from, fromAt, first);
        copy(u8, block + kRingData, from, fromAt + first, size - first);
    };
    const ringCopyOut = (block, at, to, toAt, size) => {
        const capacity = capacityOf(block);
        at %= capacity;
        const first = Math.min(size, capacity - at);
        copy(to, toAt, u8, block + kRingData + at, first);
        copy(to, toAt + first, u8, block + kRingData, size - first);
    };
    // A record's length, four bytes little endian.
    const sizeBytes = new Uint8Array(4);
    const sizeWord = new Uint32Array(sizeBytes.buffer);
    const refuseHolds = () => {
        fail(kProcessorHolds);
        return 0;
    };
    // Pops the head record into to at out if it fits capacity, or with to null drops it if it
    // does not, and notifies space. Answers its size plus one, 0 when empty or a drop keeps it.
    const pop = (ring, to, out, capacity) => {
        const block = pin(ring, kKindRing);
        if (block === 0) {
            return 0;
        }
        let signal = 0;
        let size = 0;
        lock(block + kHeaderLock);
        try {
            const used = u32[(block + kRingUsed) >>> 2];
            if (used === 0) {
                return 0;
            }
            const head = u32[(block + kRingHead) >>> 2];
            ringCopyOut(block, head, sizeBytes, 0, 4);
            size = sizeWord[0];
            if ((size & kRecordHolds) !== 0) {
                return refuseHolds();
            }
            if (to === null) {
                if (size <= capacity) {
                    return 0;
                }
            } else if (out === 0 || size > capacity) {
                return size + 1;
            } else {
                ringCopyOut(block, head + 4, to, out, size);
            }
            u32[(block + kRingHead) >>> 2] = (head + 4 + size) % capacityOf(block);
            u32[(block + kRingUsed) >>> 2] = used - 4 - size;
            signal = u32[(block + kRingSpaceSignal) >>> 2];
        } finally {
            unlock(block + kHeaderLock);
            unpin();
        }
        notify(signal);
        return size + 1;
    };

    // A region word's index, or -1 outside the region.
    const word = (block, offset) => {
        offset >>>= 0;
        if ((offset & 3) !== 0 || offset + 4 > capacityOf(block)) {
            return -1;
        }
        return (block + kRegionData + offset) >>> 2;
    };
    // -1 outside the region or once it is gone, which the std throws for.
    const atomic = (region, offset, use) => {
        const block = pin(region, kKindRegion);
        if (block === 0) {
            return -1;
        }
        try {
            const at = word(block, offset);
            return at < 0 ? -1 : use(at) >>> 0;
        } finally {
            unpin();
        }
    };

    return {
        'region_create': () => 0,
        'region_size': (region) => {
            const block = pin(region, kKindRegion);
            if (block === 0) {
                return 0;
            }
            const size = capacityOf(block);
            unpin();
            return size;
        },
        'region_lock': () => kRegionLockGone,
        'region_unlock': () => {},
        'region_read': () => 0,
        'region_write': () => 0,
        'region_poisoned': (region) => {
            const block = pin(region, kKindRegion);
            if (block === 0) {
                return 0;
            }
            const poisoned = u32[(block + kRegionPoisoned) >>> 2] !== 0 ? 1 : 0;
            unpin();
            return poisoned;
        },
        'atomic_load': (region, offset) => atomic(region, offset, (at) => Atomics.load(i32, at)),
        'atomic_store': (region, offset, value) =>
            atomic(region, offset, (at) => {
                Atomics.store(i32, at, value | 0);
                return 0;
            }),
        'atomic_add': (region, offset, value) =>
            atomic(region, offset, (at) => Atomics.add(i32, at, value | 0)),
        'atomic_cas': (region, offset, expected, desired) =>
            atomic(region, offset, (at) =>
                Atomics.compareExchange(i32, at, expected | 0, desired | 0)
            ),
        'ring_create': () => 0,
        'ring_write': (ring, data, dataByteCount, holds, holdCount) => {
            data >>>= 0;
            dataByteCount >>>= 0;
            const from = moduleRange(data, dataByteCount);
            const block = from !== null ? pin(ring, kKindRing) : 0;
            if (block === 0) {
                return kRingWriteFull;
            }
            if (holdCount !== 0) {
                unpin();
                return refuseHolds();
            }
            let signal = 0;
            lock(block + kHeaderLock);
            try {
                const capacity = capacityOf(block);
                if (4 + dataByteCount > capacity) {
                    return kRingWriteNeverFits;
                }
                const used = u32[(block + kRingUsed) >>> 2];
                if (used + 4 + dataByteCount > capacity) {
                    return kRingWriteFull;
                }
                const head = u32[(block + kRingHead) >>> 2];
                sizeWord[0] = dataByteCount;
                ringCopyIn(block, (head + used) % capacity, sizeBytes, 0, 4);
                ringCopyIn(block, (head + used + 4) % capacity, from, data, dataByteCount);
                u32[(block + kRingUsed) >>> 2] = used + 4 + dataByteCount;
                signal = u32[(block + kRingDataSignal) >>> 2];
            } finally {
                unlock(block + kHeaderLock);
                unpin();
            }
            notify(signal);
            return kRingWriteWritten;
        },
        'ring_read': (ring, out, outByteCount) => {
            out >>>= 0;
            outByteCount >>>= 0;
            const to = moduleRange(out, outByteCount);
            return to !== null ? pop(ring, to, out, outByteCount) : 0;
        },
        'ring_drop': (ring, capacity) => pop(ring, null, 0, capacity >>> 0),
        'ring_used': (ring) => {
            const block = pin(ring, kKindRing);
            if (block === 0) {
                return 0;
            }
            lock(block + kHeaderLock);
            const used = u32[(block + kRingUsed) >>> 2];
            unlock(block + kHeaderLock);
            unpin();
            return used;
        },
        'ring_signal': (ring, space) => {
            const block = pin(ring, kKindRing);
            if (block === 0) {
                return 0;
            }
            const signal = u32[(block + (space !== 0 ? kRingSpaceSignal : kRingDataSignal)) >>> 2];
            unpin();
            return signal;
        },
        'snapshot_create': () => 0,
        // As SharedSnapshot::publish: publishers take turns writing the copy
        // readers are not on.
        'snapshot_publish': (snapshot, data, dataByteCount, holds, holdCount) => {
            data >>>= 0;
            dataByteCount >>>= 0;
            const from = moduleRange(data, dataByteCount);
            const block = from !== null ? pin(snapshot, kKindSnapshot) : 0;
            if (block === 0) {
                return 0;
            }
            try {
                const capacity = capacityOf(block);
                if (holdCount !== 0) {
                    return refuseHolds();
                }
                if (dataByteCount > capacity) {
                    return 0;
                }
                lock(block + kHeaderLock);
                try {
                    const latestAt = (block + kSnapshotLatest) >>> 2;
                    const latest = Atomics.load(u32, latestAt);
                    const sizesAt = (block + kSnapshotSizes) >>> 2;
                    const replaced = latest !== 0 ? Atomics.load(u32, sizesAt + latest - 1) : 0;
                    // Nothing here could give back the replaced value's holds.
                    if ((replaced & kRecordHolds) !== 0) {
                        return refuseHolds();
                    }
                    const index = latest % kSnapshotCopies;
                    const versionAt = ((block + kSnapshotVersions) >>> 2) + index;
                    const version = Atomics.load(u32, versionAt) | 1;
                    Atomics.store(u32, versionAt, version);
                    // A load after the store keeps the bytes from landing
                    // before the odd version, as JS has no fence.
                    Atomics.load(u32, versionAt);
                    copy(u8, block + kSnapshotData + index * capacity, from, data, dataByteCount);
                    Atomics.store(u32, sizesAt + index, dataByteCount);
                    Atomics.store(u32, versionAt, version + 1);
                    Atomics.store(u32, latestAt, index + 1);
                    bumpSeq(block + kSnapshotSeq, block + kSnapshotSeqHigh);
                } finally {
                    unlock(block + kHeaderLock);
                }
                return 1;
            } finally {
                unpin();
            }
        },
        // As SharedSnapshot::read, without the lock.
        'snapshot_read': (snapshot, out, outByteCount) => {
            out >>>= 0;
            outByteCount >>>= 0;
            const to = moduleRange(out, outByteCount);
            const block = to !== null ? pin(snapshot, kKindSnapshot) : 0;
            if (block === 0) {
                return 0;
            }
            try {
                for (;;) {
                    const latest = Atomics.load(u32, (block + kSnapshotLatest) >>> 2);
                    if (latest === 0) {
                        return 0;
                    }
                    const index = latest - 1;
                    const versionAt = ((block + kSnapshotVersions) >>> 2) + index;
                    const version = Atomics.load(u32, versionAt);
                    if ((version & 1) !== 0) {
                        continue;
                    }
                    const word = Atomics.load(u32, ((block + kSnapshotSizes) >>> 2) + index);
                    const capacity = capacityOf(block);
                    const at = block + kSnapshotData + index * capacity;
                    const refused = (word & kRecordHolds) !== 0;
                    // A torn read answers what the version check discards.
                    const size = Math.min(word, capacity);
                    if (!refused && out !== 0 && size <= outByteCount) {
                        copy(to, out, u8, at, size);
                    }
                    // An exchange rather than a load keeps the copy from
                    // reading past the check.
                    if (Atomics.compareExchange(u32, versionAt, version, version) === version) {
                        return refused ? refuseHolds() : size + 1;
                    }
                }
            } finally {
                unpin();
            }
        },
        'snapshot_seq': (snapshot) => {
            const block = pin(snapshot, kKindSnapshot);
            if (block === 0) {
                return 0;
            }
            const seq = readSeq(block + kSnapshotSeq, block + kSnapshotSeqHigh);
            unpin();
            return seq;
        },
        'signal_create': () => 0,
        'signal_notify': (signal) => notify(signal),
        'signal_seq': (signal) => {
            const block = pin(signal, kKindSignal);
            if (block === 0) {
                return 0;
            }
            const seq = readSeq(block + kSignalSeq, block + kSignalSeqHigh);
            unpin();
            return seq;
        },
        'blob_create': () => 0,
        'blob_size': (blob) => {
            const block = pin(blob, kKindBlob);
            if (block === 0) {
                return 0;
            }
            const size = capacityOf(block);
            unpin();
            return size;
        },
        // A blob never changes, so the copy takes no lock.
        'blob_read': (blob, offset, out, outByteCount) => {
            offset >>>= 0;
            out >>>= 0;
            outByteCount >>>= 0;
            const to = moduleRange(out, outByteCount);
            const block = to !== null ? pin(blob, kKindBlob) : 0;
            if (block === 0) {
                return 0;
            }
            const size = capacityOf(block);
            const count = offset < size ? Math.min(outByteCount, size - offset) : 0;
            copy(to, out, u8, block + kBlobData + offset, count);
            unpin();
            return count;
        },
        'retain': () => 0,
        'release': () => {},
    };
}

// Frees the lock word and lane word a lane left held, for a page that ended
// its thread, once that thread is surely gone.
export function releaseSharedLane(heap, table, holds) {
    const token = Atomics.load(holds, kHoldToken);
    if (token < kLaneFirstToken) {
        return;
    }
    const i32 = new Int32Array(heap());
    const lockAt = Atomics.load(holds, kHoldLock);
    if (lockAt !== 0) {
        Atomics.compareExchange(i32, lockAt >>> 2, token, 0);
    }
    // Last: the slot it pinned keeps the locked block alive until here.
    if (token < kOverflowToken) {
        Atomics.store(i32, (((table >>> 0) + kTableLanes) >>> 2) + token - kLaneFirstToken, 0);
    }
}
