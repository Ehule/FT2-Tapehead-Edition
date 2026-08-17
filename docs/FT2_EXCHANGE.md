# TapeSister ↔ FT2 Tapehead exchange

Tapehead and TapeSister exchange WAV files through a shared directory. They do
not link to, inspect, or command one another's runtime state. Version 1 is
compatible with the contract implemented by TapeSister draft PR #34.

## Configure both applications

Set TapeSister's **FT2 Exchange Path** and Tapehead's `ExchangePath` to the
same existing directory. In `tapehead.ini` beside the Tapehead executable:

```ini
[TapeSister]
ExchangePath=/path/to/shared/exchange
ExecutablePath=/path/to/TapeSister
```

On Windows, use normal Windows paths. `ExecutablePath` is optional. If it is
blank, Tapehead publishes a complete folder but does not try to start
TapeSister. Tapehead never launches a path read from a manifest.

## Receive from TapeSister

Tapehead checks the exchange root at startup and approximately once per second
when no loader, modal dialog, text editor, or sampling operation owns the UI.
It stages only complete `sender=tapesister`, `recipient=tapehead` folders.
Nothing is imported automatically.

The confirmation flow shows the transfer folder, layout, sample count, every
proposed instrument/sample destination, and occupied-instrument conflicts.
Choose **Later** to leave both the folder and module untouched and suppress
that folder for this session. Right-click **Instrument Editor** and choose
**Check inbox** to reopen deferred work manually.

For `instrument_samples`, select one destination instrument. Manifest sample
numbers map to those exact slots, including sparse mappings. Importing replaces
the whole chosen instrument and clears sample slots not listed in the transfer.
Tapehead suggests a compatible empty instrument where practical.

For `separate_instruments`, select the starting destination instrument.
Manifest instrument numbers are relative positions, so gaps are preserved.
The range is rejected if any resolved instrument exceeds 128. Acceptance
replaces only the exact displayed instruments.

Tapehead decodes every WAV and constructs every replacement instrument before
opening one multi-instrument Undo transaction and committing under a single
mixer lock. A missing or corrupt WAV makes no module or Undo change. After a
successful complete commit, Tapehead atomically writes `tapehead.received`;
acknowledged folders are not offered again.

## Send to TapeSister

Right-click the **Instrument Editor** button and choose:

- **Current instr.** exports every populated sample slot (maximum 16) from the
  current nonzero instrument. Sample slots 1–16 map to the same TapeSister
  tile numbers and the manifest uses `instrument_samples`.
- **Instr. range** walks forward from the current instrument, collects up to
  16 occupied instruments, and deterministically exports the first populated
  sample slot in each. Tiles are assigned sequentially and the manifest uses
  `separate_instruments`.

Tapehead previews every `instrument:sample → tile` mapping. After confirmation
it creates a unique `tapehead_to_tapesister_NNNNNN.partial` folder, writes all
WAVs through Tapehead's normal WAV writer, writes the manifest last, and then
renames the folder to remove `.partial`. Existing transfers are never
overwritten. Failure cleans up only the partial folder created by that action.

## Authoritative version-1 manifest

Each complete transfer folder contains `exchange.tsexchange`:

```text
TAPESISTER_EXCHANGE 1
sender=tapesister
recipient=tapehead
layout=instrument_samples
count=2
item=1,0,1,01_Kick.wav
item=4,0,4,04_Noise.wav
```

An item is:

```text
tapesister_tile,ft2_instrument,ft2_sample,wav_filename
```

All numbers are one-based except the `0` FT2 instrument in an outgoing
TapeSister `instrument_samples` transfer; Tapehead replaces it with the
destination selected in the confirmation dialog. Valid layouts are
`instrument_samples` and `separate_instruments`. In TapeSister's
`separate_instruments` offers, instrument values are relative positions within
the transfer. Tapehead's outgoing manifests retain authoritative source FT2
instrument and sample numbers.

Version 1 permits at most 16 items. Tapehead rejects unsupported versions or
layouts, malformed counts, out-of-range values, duplicate tile targets,
duplicate resolved FT2 targets, absolute WAV paths, separators, `..`, and
missing files. It ignores `.partial` folders, invalid/incomplete folders, its
own outgoing folders, and folders containing `tapehead.received`.

## WAV metadata

The normal Tapehead WAV loader adopts standard `smpl` MIDI unity note and
32-bit pitch fraction as the sample's representable C-4 rate. Forward,
ping-pong, and backward loop types round-trip through the normal WAV writer;
backward loops repeat in reverse instead of being treated as ping-pong. Invalid
loop endpoints are ignored. WAVs without `smpl` metadata keep ordinary sample
rate behavior.

## Manual round-trip checklist

1. Build TapeSister PR #34 and this Tapehead branch. Point both applications
   at a new shared exchange directory.
2. In TapeSister, publish an `instrument_samples` offer with sparse tiles (for
   example 1 and 4). Confirm Tapehead previews and imports exact sample slots
   1 and 4 into one instrument, then Undo and Redo once.
3. Repeat into an occupied instrument. Confirm the warning says the whole
   instrument is replaced and unlisted slots are cleared. Choose **Later**,
   wait several seconds, and confirm no repeated prompt; use **Check inbox** to
   reopen it.
4. Publish a `separate_instruments` offer with a relative gap. Confirm the
   preview preserves the gap, warns for each occupied destination, rejects an
   overflowing start range, and Undo/Redo treats the accepted batch as one
   operation.
5. Put a missing or corrupt WAV in a complete-looking offer. Confirm the module
   and Undo history do not change and no `tapehead.received` appears.
6. After a successful import, confirm `tapehead.received` exists and the folder
   is not offered again after restarting Tapehead.
7. In Tapehead, right-click **Instrument Editor** and send **Current instr.**
   from an instrument with sparse populated slots. Verify TapeSister previews
   the same tile numbers and accepts them.
8. Send **Instr. range** across instruments with gaps and multiple samples.
   Verify exactly the first populated slot per occupied instrument is exported
   to sequential tiles.
9. Leave `ExecutablePath` blank and confirm publishing still completes. Then
   set a valid path and confirm TapeSister starts only after the final folder
   is visible.
10. Round-trip WAVs with fine/root tuning and forward, ping-pong, and backward
    loops. Verify tuning is retained, loop endpoints remain valid, and the
    backward loop plays repeatedly in reverse.
