# Multichannel Output — Pass 3

Pass 3 repairs the live Bus B silence found during the Echo Indigo DJx JACK
hardware test. Tapehead could publish and connect `bus_B_L/R`, and its scope
marker could show `B`, while the realtime mixer still folded the sounding
voice into Bus A.

## Repair

- The JACK callback's configured output-bus count is now authoritative for
  every render cycle. It is passed directly into the mixer instead of relying
  on a separately stored device count.
- Every physical FT2 channel is rendered once into a neutral scratch buffer,
  then that audio is distributed to the channel's selected logical bus mask.
  This prevents an exclusive Bus B route from inheriting a stale Bus A mixer
  destination pointer.
- `Ctrl+Alt+Left-click` To Main duplication still copies one rendered voice to
  both buses; note, envelope, effect, sample position, and Poly/FasTracks state
  are not processed twice.
- The JACK callback's local loop variable no longer shadows FT2's global
  `channel[]` array, removing the Linux build warning reported during Pass 2.

## Regression coverage

Run:

```bash
python3 scripts/test_audio_bus_delivery.py
```

The test sends a synthetic live FT2 voice through A-only, B-only, and A+B
routes while deliberately setting the old global bus count to stereo. The
explicit two-bus render must still reach Bus B.

The existing Pass 2 setup and controls are unchanged. Use `OutputBuses=2`,
select **Tapehead JACK Virtual Outputs**, and connect `bus_B_L/R` to any known
working JACK playback pair.
