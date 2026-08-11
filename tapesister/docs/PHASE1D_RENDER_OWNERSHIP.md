# Phase 1D render and preview ownership

The render worker owns one in-flight recipe snapshot and, at most, one pending
snapshot. A request replaces the pending snapshot under the worker mutex. A
result is publishable only when its generation still equals the newest request;
stale success and failure results are destroyed by the worker.

The main thread polls completed results and publishes them to `ts_preview_pool`.
Publication retains one current-preview reference and releases only the prior
current reference. Each audition voice retains the preview embedded in its
source. The callback may only atomically release that reference and mark it
retired; `ts_preview_pool_collect` performs final sample destruction on the main
thread. Consequently an old voice continues reading its immutable generation,
while history and superseded render requests retain no audio buffers.

The callback publishes only atomic cursor age, source frame, and source length.
The main thread converts those values to a normalized waveform position. Voice
slot reuse receives a strictly increasing age, preventing a cursor from
following a stale slot identity.

The final Phase 1D pass still owns path modals and Save/Load/Bake actions,
complete saved/baked identities, Parent confirmation, direct UTF-8/numeric
entry, drag transactions, mouse-wheel polish, action buttons, and expanded SDL
smoke coverage.
