# Commercial reference: spectrum analyzer controls

Research checked 2026-11-16 against FabFilter's official Pro-Q 4 online help.

FabFilter documents three spectrum analyzer features that are not present in openTunerEQ's original analyzer:

1. **Freeze / peak hold.** Pro-Q 4's Freeze control accumulates a maximum spectrum over time. openTunerEQ now holds the highest measured levels while frozen and continues to admit new peaks. Transport stop still clears stale data only when Freeze is off.
2. **Selectable release speed.** Pro-Q 4 exposes a spectrum release-speed setting. openTunerEQ offers Slow (24 dB/s), Medium (48 dB/s), and Fast (96 dB/s), which control how quickly held bins decay when Freeze is off.
3. **Spectrum tilt.** Pro-Q 4 documents tilt around 1 kHz, with 4.5 dB/oct as its natural-looking default. openTunerEQ offers 0, 3, 4.5, and 6 dB/oct, centered at 1 kHz. Tilt is a display offset, not EQ processing.

Reference: [FabFilter Pro-Q 4 Help — Spectrum analyzer](https://www.fabfilter.com/help/pro-q/using/analyzer). The feature set is an independent implementation; it does not copy vendor code, artwork, or exact presentation.

Verification covers analyzer decay and freeze behavior, octave tilt, parameter recall/defaults for older sessions, and the corresponding native editor controls. The analyzer remains on the editor thread; audio capture still uses its existing preallocated FIFO path.
