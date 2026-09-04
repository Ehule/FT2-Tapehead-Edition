# TapeSister ↔ FT2 Tapehead exchange

Tapehead and TapeSister exchange WAV files through a shared directory. They do
not link to or command one another's project state. Each application refreshes
a small presence marker in that directory so a sender can reuse a receiver
that is already open. Version 1 is compatible with the contract implemented by
TapeSister's version-1 exchange and the version-2 All Pages contract introduced
by TapeSister PR #40.

## Configure both applications

Set TapeSister's **FT2 Exchange Path** and Tapehead's **Exchange** field to the
same existing directory. In Tapehead, open **Configuration → Layout**. Click a
path field to type or paste it, or double-click it to use Tapehead's built-in
browser. The values are stored in `tapehead.ini` beside the executable as:

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
Directory scanning, manifest parsing, file checks, and sample decoding happen
off the UI/audio path. It stages only complete `sender=tapesister`,
`recipient=tapehead` folders. Nothing is imported automatically.

The confirmation flow shows the transfer folder, layout, sample count, every
proposed instrument/sample destination, and occupied-instrument conflicts.
Choose **Later** to leave both the folder and module untouched and suppress
that folder for this session. Right-click **Instrument Editor** and choose
**Check inbox** to reopen deferred work manually.

The same right-click menu includes **Open folder** (`O`). It opens Tapehead's
Disk Op screen in Sample mode at the configured exchange root, even when no
unacknowledged transfer remains.

For `instrument_samples`, select one destination instrument. Manifest sample
numbers map to those exact slots, including sparse mappings. Importing replaces
the whole chosen instrument and clears sample slots not listed in the transfer.
Tapehead suggests a compatible empty instrument where practical.

For `separate_instruments`, select the starting destination instrument.
Manifest instrument numbers are relative positions, so gaps are preserved.
The range is rejected if any resolved instrument exceeds 128. Acceptance
replaces only the exact displayed instruments.

For version-2 `page_instruments`, Tapehead identifies the offer as a multi-page
TapeSister bank and shows its page/instrument span, total WAV count, first and
last destination instruments, individual page/tile mappings, and occupied
sample-slot warnings. Select the starting instrument: page 1 maps there, page
2 maps to the next instrument, and so on. Each tile retains the same sample
number. Only listed sample slots are updated; empty TapeSister tiles and sparse
pages do not clear existing samples or instrument settings.

Tapehead decodes every WAV and constructs every replacement instrument before
opening one multi-instrument Undo transaction and committing under a single
mixer lock. A missing or corrupt WAV makes no module or Undo change. After a
successful complete commit, Tapehead atomically writes `tapehead.received`;
acknowledged folders are not offered again.

## Send to TapeSister

Right-click the **Instrument Editor** button, choose **Send samples**, then
choose:

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

Both applications refresh advisory `.tapehead.running` and
`.tapesister.running` markers once per second. A normal **Publish** reuses a
live TapeSister and lets its inbox poll discover the new transfer instead of
launching another process. **Publish + New** deliberately starts another
TapeSister instance after publication. A stale or missing presence marker
falls back to the normal configured-executable launch.

## Render tracker audio to TapeSister

Right-click **Instrument Editor**, choose **Render audio**, and select one of
five scopes. The next dialog chooses either an ordinary WAV in `Captures` or
an explicit TapeSister publication:

- **Block** renders the selected Pattern Editor rectangle as one literal
  cycle. Only selected tracks are audible; Fxx, Gxx, Hxx, and EEx commands on
  the same rows outside the rectangle still govern timing/global state.

- **Pattern mix** renders the pattern assigned to the current song order from
  its first row through its end.
- **Pattern track** renders that same pattern with only the currently selected
  tracker track audible.
- **Song track** renders orders `00` through the end of the song with only the
  selected tracker track audible.
- **Song mix** renders orders `00` through the end of the song as a stereo mix.

Pattern/song track isolation keeps all tracker channels running internally so tempo, speed,
pattern-flow, and other global commands on non-audible tracks still govern the
render. The output uses the WAV exporter's current sample rate, bit depth, and
amplification settings and is always stereo. The confirmation dialog displays
the exact order/pattern or song range, selected track where relevant, format,
and destination before rendering begins.

The literal block transport deliberately ignores FastTracks ratios, LEN track
lengths, Pattern/Poly Matrix routing, Bxx/Dxx jumps, E6x loops, and Zxx
FastTracks commands. Selecting rows 16-31 and tracks 3-5 therefore renders
exactly those cells in lockstep, regardless of the surrounding composition.

For the fast version, select a block and press **Ctrl+L**. Tapehead loops only
that rectangle. **Shift+Arrow** moves the selection's active corner while it
plays; the new bounds take effect at the next loop seam. Press plain **F8**
during Block Loop to render exactly one current cycle to the auto-created
`Captures` folder, then resume Block Loop. Modified F8 commands retain their
existing transpose meanings, and plain F8 retains block extraction whenever
Block Loop is off.

Plain **F7** turns Block Loop into a quantized performance recorder. The first
press arms capture; recording begins at the next loop seam. Change the rows or
tracks with **Shift+Arrow**, hold a shape for several repeats, or continue
performing without a fixed cycle count. Press **F7** again and Tapehead records
through the current cycle, stops at its closing seam, saves
`Song_BlockPerformance_###.wav` in `Captures`, and leaves Block Loop running.
Pressing F7 a second time before the armed start cancels it. Stopping Block
Loop while recording safely closes what has already been captured; stopping
while merely armed keeps no file.

Performance capture records the live post-mixer Bus A stereo stream at the
active audio-device rate and current WAV bit depth. Its audio callback only
copies into a bounded memory ring; a background writer performs all file I/O
through a `.partial` file and publishes the final WAV only after a clean
close. F8 remains the deterministic one-cycle offline capture, while F7 is
the evolving, what-you-hear performance path. Modified F7 commands retain
their existing transpose meanings, and plain F7 remains octave 6 outside
Block Loop.

Each successful render sent to TapeSister becomes a normal one-item version-1
`instrument_samples` offer and therefore works with current TapeSister builds
without a protocol change. TapeSister receives the WAV in tile 1. The transfer
also contains `render.tapehead`, a versioned text sidecar recording render
scope, order range, pattern or `-1` for a song render, selected track or `0`
for a mix, source channel count, format, initial tempo/speed, frame count, and
duration. Current TapeSister builds safely ignore that sidecar; it preserves
enough provenance for future placement and round-trip features.

Tapehead writes the WAV first, the sidecar second, and the ordinary manifest
last inside the unique `.partial` folder. Only after the renderer and every
file close successfully does Tapehead rename the folder into view. Cancelling,
stopping, an I/O error, or a failed render removes the pending folder and never
publishes a complete offer. **Publish** and **Publish + New** retain the same
running-instance behavior as sample sends. Tapehead also refuses to publish a
render above current TapeSister's 100,000,000-frame WAV import ceiling; lower
the WAV sample rate or render a shorter scope if that limit is reached.

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

## Version-2 All Pages manifest

TapeSister PR #40's **FT2 Link → All Pages** action publishes:

```text
TAPESISTER_EXCHANGE 2
sender=tapesister
recipient=tapehead
layout=page_instruments
count=3
item=1,1,1,P001_01_Kick.wav
item=4,1,4,P001_04_Noise.wav
item=1,2,1,P002_01_Bass.wav
```

Each item is
`tapesister_tile,relative_ft2_instrument,ft2_sample,wav_filename`. Relative
instrument 1 is TapeSister page 1. Repeated tile/sample numbers are valid on
different pages, but a duplicate `(relative instrument, sample)` is rejected.
Relative instruments may be 1–255 and each page may address sample slots 1–16;
the complete resolved range must still fit Tapehead's 128 instruments. `count`
is the exact number of item/WAV rows, not the number of pages.

Version 2 accepts only `page_instruments` from TapeSister to Tapehead. Version
1 remains authoritative for `instrument_samples` and `separate_instruments`.
Absolute paths, separators, `..`, malformed/duplicate fields, unsafe names,
missing files, non-WAV content, corrupt WAVs, duplicate destinations, and
out-of-range mappings reject the whole offer. No acknowledgement is written
unless every WAV stages and the single Undo transaction commits.

## WAV metadata

The normal Tapehead WAV loader adopts standard `smpl` MIDI unity note and
32-bit pitch fraction as the sample's representable C-4 rate. Forward,
ping-pong, and backward loop types round-trip through the normal WAV writer;
backward loops repeat in reverse instead of being treated as ping-pong. Invalid
loop endpoints are ignored. WAVs without `smpl` metadata keep ordinary sample
rate behavior.

## Manual round-trip checklist

1. Build TapeSister PR #40 and this Tapehead branch. Point both applications
   at a new shared exchange directory.
2. Fill at least two Sample Bank pages in TapeSister, including the same tile
   number on both pages and some empty slots. Choose **FT2 Link → All Pages**.
   Confirm Tapehead reports a multi-page bank and the correct page/WAV counts.
3. Choose a starting instrument other than 1. Confirm page 1 maps there, page 2
   maps to the next instrument, repeated tile numbers land in the same-numbered
   sample slot of their respective instruments, and empty slots preserve any
   existing destination samples. Undo and Redo once for the whole transfer.
4. Repeat across an occupied range. Confirm listed occupied slots are warned
   about, **Later** leaves the song and transfer untouched, no automatic repeat
   prompt appears that session, and **Check inbox** reopens the offer.
5. In TapeSister, publish an `instrument_samples` offer with sparse tiles (for
   example 1 and 4). Confirm Tapehead previews and imports exact sample slots
   1 and 4 into one instrument, then Undo and Redo once.
6. Repeat into an occupied instrument. Confirm the warning says the whole
   instrument is replaced and unlisted slots are cleared. Choose **Later**,
   wait several seconds, and confirm no repeated prompt; use **Check inbox** to
   reopen it.
7. Publish a `separate_instruments` offer with a relative gap. Confirm the
   preview preserves the gap, warns for each occupied destination, rejects an
   overflowing start range, and Undo/Redo treats the accepted batch as one
   operation.
8. Put a missing or corrupt WAV in a complete-looking offer. Confirm the module
   and Undo history do not change and no `tapehead.received` appears.
9. After a successful import, confirm `tapehead.received` exists and the folder
   is not offered again after restarting Tapehead.
10. In Tapehead, right-click **Instrument Editor**, choose **Send samples**,
   and send **Current instr.** from an instrument with sparse populated slots.
   Verify TapeSister previews the same tile numbers and accepts them.
11. Send **Instr. range** across instruments with gaps and multiple samples.
   Verify exactly the first populated slot per occupied instrument is exported
   to sequential tiles.
12. With TapeSister already open, send normally and confirm no second instance
   opens. Repeat with **Publish + New** and confirm another instance opens.
   Close TapeSister, wait over five seconds, and confirm normal publishing
   launches it only after the final folder is visible. A blank
   `ExecutablePath` must still allow publication.
13. Round-trip WAVs with C4 unity pitch, fine/root tuning, and forward,
    ping-pong, and backward
    loops. Verify tuning is retained, loop endpoints remain valid, and the
    backward loop plays repeatedly in reverse.
14. Put the cursor on a track with obvious audio and choose **Render audio →
    Pattern track**. Confirm the dialog identifies the correct order, pattern,
    and one-based track; accept it and verify TapeSister receives a stereo WAV
    in tile 1 containing only that track for one pattern.
15. Repeat **Pattern mix**, **Song track**, and **Song mix**. Confirm pattern
    renders stop after the current order, song renders cover the full order
    range, mix renders include all audible tracks, and isolated renders still
    honor tempo/speed commands carried by other tracks.
16. During a longer render, stop the WAV render. Confirm no completed exchange
    folder appears and the temporary `.partial` folder is removed. Then finish
    a render and verify its folder contains the WAV, `render.tapehead`, and
    `exchange.tsexchange`, with the manifest mapping the WAV to tile 1.
