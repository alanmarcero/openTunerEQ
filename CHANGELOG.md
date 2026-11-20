# Changelog

## v1.9.0

- **Matched & ZDF Filters:** Upgraded low-pass filters to reach `-inf dB` at Nyquist without stopband leakage, and switched to click-free ZDF TPT SVFs across fast pitch-snapped sweeps.
- **Musical Bell & Shelf Shaping:** Added resonant Pultec-style low/high shelves with user Q, symmetric root-Q bell curves, and phase-compensated Mid/Side/Left/Right band placement.
- **Smooth Dynamics & Output:** Added primed equal-power topology crossfades, S-curve bypass fades, auto-gain compensation, and antialiased asymmetric soft saturation at the output ceiling.
