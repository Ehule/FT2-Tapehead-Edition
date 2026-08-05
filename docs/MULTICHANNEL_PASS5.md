# Multichannel Pass 5 — live routing fix

> **Historical repair record.** This fix is included in CP04.6. Current setup,
> including later Mono Outputs behavior, is in
> [`MULTICHANNEL_OUTPUT.md`](MULTICHANNEL_OUTPUT.md).

Pass 5 fixes the ordinary tracker-audio failure isolated by the X220, native
JACK, and Echo Indigo DJx hardware test.

The decisive Pass 4B traces showed:

```text
Track on A: A=0.195136 B=0.000000 | 1=0x0001
Track on B: A=0.000000 B=0.000000 | 1=0x0002
```

That proved the scope control and route mask were correct, and the Bus B JACK
port itself had already passed the injected-tone test. The live voice was being
lost between FT2's per-channel mixer and the logical Bus B buffer.

Pass 5 combines the working parts of the earlier designs:

- JACK's configured bus count remains authoritative for every render.
- A voice assigned exclusively to A, B, or another single bus is mixed directly
  into that bus's buffer.
- A deliberate multi-destination route such as A+B still renders once into the
  neutral channel buffer and then duplicates the resulting samples.
- Stereo devices and offline WAV rendering continue to fold logical routes to A.

## Hardware confirmation

Run Tapehead on the same native JACK server as QjackCtl. Do not prefix the
command with `pw-jack` when QjackCtl is controlling native JACK.

```bash
TAPEHEAD_JACK_DEBUG=1 ./release/other/ft2-clone
```

Use four clean live Graph connections:

```text
bus_A_L -> playback_1
bus_A_R -> playback_2
bus_B_L -> playback_3
bus_B_R -> playback_4
```

Disable QjackCtl's persistent Patchbay profile while making the test, so it
cannot silently restore cross-connections. With a sounding track switched from
A to B, the terminal should change from an A peak to a B peak while its route
changes from `0x0001` to `0x0002`.

The real-time scheduling warning printed by JACK on an unconfigured desktop
system is not a multichannel-routing failure.
