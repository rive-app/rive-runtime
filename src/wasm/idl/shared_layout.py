"""Single source for the words shared memory lives in, which librive and the
lanes that reach it through Atomics (web/rive_shared_lane.mjs) both read.

Offsets are wasm32 bytes. generate.py projects this table into
include/rive/wasm/wasm_shared_layout_gen.hpp, whose constants wasm_shared.cpp
uses and static asserts its structs against, and web/rive_shared_layout.mjs.
"""

ID_SLOT_SHIFT = 12
CHUNK_SHIFT = 10
LANE_COUNT = 32
SLOT_COUNT = 1 << (31 - ID_SLOT_SHIFT)
MAX_OBJECTS = 65536

# (group, what it is, [(name, value)]); each entry becomes k<Group><Name>.
SHARED_LAYOUT = [
    ('Kind', 'What a block holds, its first word.', [
        ('region', 0),
        ('ring', 1),
        ('snapshot', 2),
        ('signal', 3),
        ('blob', 4),
    ]),
    ('Header', 'Every block starts with these words.', [
        ('kind', 0),
        # 0 when free, else its holder's token: 1 for librive, a lane's own
        # token for a lane without librive.
        ('lock', 4),
        # Bytes after the kind's block.
        ('capacity', 8),
        # Set under the lock once the object retired: a ring or snapshot
        # takes no more records, which would carry holds nobody gives back.
        ('closed', 12),
        ('size', 16),
    ]),
    ('Region', 'A region after its header.', [
        ('poisoned', 16),
        ('data', 32),
    ]),
    ('Ring', 'A ring after its header; signals are registry ids.', [
        ('head', 16),
        ('used', 20),
        ('dataSignal', 24),
        ('spaceSignal', 28),
        ('data', 32),
    ]),
    ('Snapshot', 'A snapshot after its header: copies of capacity bytes'
     ' each, a version per copy that is odd while a publish writes it.', [
        # The copy holding the latest value plus one, 0 before any publish.
        ('latest', 16),
        # A count in two words, so a lane bumps it without 64 bit atomics.
        ('seq', 24),
        ('seqHigh', 28),
        ('versions', 32),
        ('sizes', 40),
        ('data', 48),
        ('copies', 2),
    ]),
    ('Record', 'A ring record\'s length word or a snapshot copy\'s size.'
     ' With the holds bit set, the bytes end in the shared ids the value'
     ' holds and then their count.', [
        ('holds', 0x80000000),
    ]),
    ('Signal', 'A signal after its header; watchers point at wake words.', [
        ('watcherCount', 16),
        ('watcherCapacity', 20),
        ('seq', 24),
        ('seqHigh', 28),
        ('watchers', 32),
    ]),
    ('Blob', 'A blob after its header: bytes fixed once made.', [
        ('data', 16),
    ]),
    ('WakeWord', 'The futex word a watcher parks on.', [
        ('word', 0),
    ]),
    ('Table', 'The registry, at SharedRegistry::table().', [
        ('lock', 0),
        ('count', 4),
        ('freeHead', 8),
        ('freeTail', 12),
        # A word per lane without librive: 0 unclaimed, idle, or the slot
        # it has pinned.
        ('lanes', 16),
        # How many lanes took a token past the lane words.
        ('tokens', 16 + 4 * LANE_COUNT),
        ('chunks', 20 + 4 * LANE_COUNT),
    ]),
    ('Slot', 'A registry slot. Its state packs the generation in the low'
     ' bits, a live bit, then the pins librive holds.', [
        ('state', 0),
        ('block', 4),
        # The VM that made it, the only one whose lanes find it, in a low and
        # a high word.
        ('owner', 16),
        ('ownerHigh', 20),
        ('size', 24),
        ('live', 1 << ID_SLOT_SHIFT),
        ('pin', 1 << (ID_SLOT_SHIFT + 1)),
    ]),
    ('Chunk', 'Slots come in chunks that never move once made.', [
        ('shift', CHUNK_SHIFT),
        ('slots', 1 << CHUNK_SHIFT),
        ('count', SLOT_COUNT >> CHUNK_SHIFT),
    ]),
    ('Id', 'An id: the top bit, its slot, the generation\'s low bits.', [
        ('bit', 0x80000000),
        ('slotShift', ID_SLOT_SHIFT),
        ('generationMask', (1 << ID_SLOT_SHIFT) - 1),
    ]),
    ('Lane', 'Lanes without librive.', [
        ('count', LANE_COUNT),
        ('idle', 1),
        # A lane's token is its lane word's index plus this.
        ('firstToken', 2),
    ]),
    ('Shared', 'The most bytes a shared object holds, and the most objects'
     ' one script keeps at once, so no file fills the registry.', [
        ('maxBytes', 256 << 20),
        ('maxObjects', MAX_OBJECTS),
    ]),
    ('RingWrite', 'What ring_write answers.', [
        # The ring is full right now, or gone.
        ('full', 0),
        ('written', 1),
        # Larger than the whole ring, so it never fits.
        ('neverFits', 2),
    ]),
    ('Create', 'What a create answers instead of an id.', [
        # On an audio processor, or on a job for anything but a blob.
        ('refused', 0),
        ('full', 1),
        ('badSize', 2),
        # The creator's shared memory budget, or the heap, ran out.
        ('noMemory', 3),
        # The creator keeps kSharedMaxObjects already.
        ('tooMany', 4),
    ]),
    ('RegionLock', 'What region_lock answers.', [
        ('gone', 0),
        ('acquired', 1),
        ('nested', 2),
        ('timedOut', 3),
    ]),
    ('Hold', 'Words of the counter an NRDP page shares with its worklet'
     ' thread, so it can free what the thread held when it ends it.', [
        # Bumped every block.
        ('blocks', 0),
        # 1 while a processor starts.
        ('starting', 1),
        ('token', 2),
        # The lock word the lane is taking or holds.
        ('lock', 3),
        ('words', 4),
    ]),
]

# (name, text): what the std throws for a create's answer, after the API name.
SHARED_TEXTS = [
    ('sharedTooMany', 'a script keeps at most %d shared objects at once, a'
     ' ring counting three; release shared objects once done with them'
     % MAX_OBJECTS),
]
