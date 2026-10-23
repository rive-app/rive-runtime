"""Single source for the numbers lanes and their hosts agree on: the lane
kinds and the module sections they ride in, the states every lane moves
through, and the codes natives answer in place of one.

generate.py projects it into include/rive/wasm/wasm_lane_layout_gen.hpp,
which librive and the bake use, rasc's lane_imports_gen.h,
web/rive_lane_imports.mjs and std/rive/lane_codes.as, and checks that the std
host shims declare the exports it names.
"""

# (kind, the main module section its lane module rides in, the prefix each
# entry's number follows), in rive_lanes_v1's order.
LANE_KINDS = [
    ('job', 'rive.jobs', '__rive_job_'),
    ('worker', 'rive.workers', '__rive_worker_'),
    ('processor', 'rive.audio', '__rive_processor_'),
]

# A kind's section with this added names its entries in order, as a wasm name
# section does: the processor class, or job or worker function.
LANE_NAMES_SUFFIX = '.names'

# (name, export, the std host shim that declares it or None when the bake
# writes it), the exports a lane module may carry besides its entries.
LANE_EXPORTS = [
    # Every lane: where the host copies the input, and its release.
    ('laneAlloc', '__rive_lane_alloc', 'lane_host.as'),
    ('laneFree', '__rive_lane_free', 'lane_host.as'),
    # Every lane: the mark that asks for the native watchdog, and the check a
    # metered lane calls, which a lane without fuel leaves out.
    ('hostBudget', '__riveHostBudget', 'lane_host.as'),
    ('fuelCheck', '__riveFuelCheck', 'lane_host.as'),
    ('workerWake', '__rive_worker_wake', 'worker_host.as'),
    ('processorProcess', '__rive_processor_process', 'processor_host.as'),
    # A worker's or processor's minor collection, at each return to the host.
    ('frameMinor', '__riveFrameMinor', None),
    # The module's start function, which the host moves out of instantiation
    # and calls itself.
    ('liftedStart', '__riveLiftedStart', None),
    # A job module puts its instance back as instantiation left it and runs
    # its start function again, so the host may run another job in it.
    ('laneReset', '__riveLaneReset', None),
]

# (name, value, what it means): what an awaited wait settles with in place
# of a signal sequence, which is never negative.
WAIT_SETTLES = [
    ('timedOut', -1, 'What an awaited wait settles with when it timed out.'),
    ('stopping', -2, 'What an awaited wait settles with inside a worker'
     ' being stopped.'),
    ('gone', -3, 'What an awaited wait settles with when its signal is gone,'
     ' as when the ring or port it waits on was released.'),
]

# (name, value, what it means): what a blocking wait answers besides those.
WAIT_ANSWERS = [
    ('refused', -4, 'What wait_within answers when the waiter holds a shared'
     ' region.'),
]

# A processor block's budget in ms over its call, copy out and collection:
# hardBlocks blocks of its frames, at least 1, or 100 for the warm up block.
# 0, for no budget, when hardBlocks is 0.
BLOCK_BUDGET_MS = ('hardBlocks == 0 ? 0.0 : max(first ? 100.0 : 1.0,'
                   ' ceil(hardBlocks * 1000.0 * frames / sampleRate))')

# (name, text): what lanes report the same way on every host.
LANE_TEXTS = [
    ('processorHolds', 'a processor cannot pass shared objects through rings'
     ' or snapshots, as the audio thread may not hold or give them back; pass'
     ' them in Audio.start\'s argument'),
    ('processorOverran', 'processor overran its block budget'),
    ('processorInitOverran', 'processor init overran its budget'),
    # The audio worklet node every processor of a page runs in.
    ('processorNode', 'rive-processors'),
]

# (state, what it means), numbered in order; the std's LaneState.
LANE_STATES = [
    ('starting', 'Started but not yet running: a job waiting for a thread, a'
     ' worker before its first await, a processor before its init ran.'),
    ('running', 'Running: a job on a thread, a worker between awaits, a'
     ' processor in the mix.'),
    ('done', 'Returned: a job\'s result is ready, a worker\'s promise'
     ' resolved with its result.'),
    ('failed', 'Threw, trapped or, for a processor, overran its hard limit;'
     ' `failed` says why.'),
    ('stopped', 'Ended early by a stop, a cancelled promise or a release.'),
]

# (group, what it is, [(name, value)]); each entry becomes k<Group><Name>.
LANE_LAYOUT = [
    ('Join', 'What join answers instead of a state when the joiner holds a'
     ' shared region.', [
         ('refused', 5),
     ]),
    ('Start', 'What start answers instead of a lane when the section is'
     ' there but its module failed to load; failed then answers the load'
     ' error for it.', [
         ('failed', 0xffffffff),
     ]),
    ('JoinAny', 'What join_any answers when the joiner holds a shared'
     ' region.', [
         ('refused', 0xffffffff),
     ]),
    ('ProcessorEvent', 'What a processor event carries for a console line,'
     ' in place of a state.', [
         ('log', 0xffffffff),
     ]),
    ('Reuse', 'The most 64 KiB pages a job instance\'s memory may have grown'
     ' to for another job to reuse it, as memory never shrinks.', [
         ('maxPages', 256),
     ]),
    ('Processor', 'How long a processor\'s init may run in milliseconds,'
     ' natively per step and in a worklet as a whole, far longer than a block'
     ' as it may build tables.', [
         ('initMs', 1000),
     ]),
]
