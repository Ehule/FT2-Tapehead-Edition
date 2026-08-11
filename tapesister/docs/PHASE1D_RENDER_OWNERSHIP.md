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

Live audition applies fixed `0.22` gain per voice before summing, leaving useful
headroom for ordinary chords. The existing final finite check and hard bounds
remain only as pathological protection; rendered previews and baked WAV bytes
are not modified.

Each accepted factory/file session increments a separate session identity and
clears Undo/Redo only after its candidate render succeeds. A published preview
is eligible for new notes only when its publication session equals the current
working session. Old voices keep their retained preview, but note-on is rejected
while the new session render is pending or failed; this prevents recipe A from
auditioning as newly triggered recipe B.

Save replacement stages a sibling before moving the old target to a sibling
backup. Pair replacement stages and validates both files, backs up every old
target, and restores the complete old pair if either publication fails. Saved
and Baked identities change only after their whole transaction succeeds.

The portable-code checkpoint still requires the X220 SDL build, bounded smoke
workflow, screenshot inspection, audio validation, and manual checklist before
an unconditional Phase 1D completion claim.

## Rejected edits and replacement recovery

Recipe edits remain provisional until their matching generation renders. The
application snapshots both the last accepted recipe and its complete history.
A matching render failure restores those snapshots, retains the last audible
preview, and reports `REJECTED`; session and generation checks prevent an old
failure from rolling back newer work.

Confirmed file replacement distinguishes ordinary publication failure (all
originals restored) from incomplete recovery. Incomplete recovery returns
`TS_IO_ROLLBACK_FAILED` and retains the affected backup sibling for manual
recovery; Saved and Baked identities are not advanced on either failure.
