# Multichannel Pass 4B diagnostic

> **Historical diagnostic only.** The injected test tone and environment
> variables below were used to isolate the fault fixed in Pass 5. They are not
> required for normal CP04.6 routing. See
> [`MULTICHANNEL_OUTPUT.md`](MULTICHANNEL_OUTPUT.md).

This build does not claim to fix the remaining silent Bus B problem. It adds
live measurements of the actual JACK port buffers and an optional test tone
injected directly into a selected JACK bus. Pass 4B corrects the original Pass
4 meter, which measured the tracker mix before the diagnostic tone was added.

Build normally:

```bash
./make-linux.sh
```

For the decisive Bus B test, connect `bus_B_L/R` to the already proven
`playback_1/2` pair and launch from a terminal with:

```bash
TAPEHEAD_JACK_DEBUG=1 TAPEHEAD_JACK_TEST_BUS=B \
pw-jack ./release/other/ft2-clone
```

Select `Tapehead JACK Virtual Outputs` in Config > Audio if it is not already
selected. The build injects a quiet 440 Hz tone directly into Bus B after the
tracker mixer. The terminal also prints a report once per second, for example:

```text
Tapehead JACK live peaks: A=0.000000 B=0.312500 | routes: 1=0x0002 2=0x0001
```

Interpretation:

- If the report shows a Bus B peak near `0.125000`, Tapehead wrote the tone into
  the actual Bus B JACK port buffer. If it is nevertheless inaudible, inspect
  the live JACK graph connection after restarting Tapehead.
- If Bus B still reports `0.000000`, Tapehead did not write the Bus B port
  buffer and the fault remains inside the native backend.
- If the 440 Hz tone is audible, the Bus B JACK port and downstream connection
  work. Stop the program, rerun without `TAPEHEAD_JACK_TEST_BUS=B`, route the
  sounding tracker channel to B, and copy several live-peak lines.
- In the route list, `0x0001` means Bus A, `0x0002` means Bus B, and `0x0003`
  means A+B.

The environment variables affect only the process launched with them. Starting
Tapehead normally restores ordinary behavior.
