# Changelog

## v1.9.2

- **Parameter Locking:** Add per-parameter locking across every plugin parameter, persisted in session state and preserved across preset loads and randomization.

## v1.9.1

- **UI & Suite Fixes:** UI polish, unit formatting and typed-value entry, inactive control dimming, per-slot A/B undo, inline preset confirms, peak-hold meters, uniform scaling, and screen-reader titles.

## v1.9.0

- **Matched & ZDF Filters:** Upgraded low-pass filters to reach `-inf dB` at Nyquist without stopband leakage, and switched to click-free ZDF TPT SVFs across fast pitch-snapped sweeps.
- **Musical Bell & Shelf Shaping:** Added resonant Pultec-style low/high shelves with user Q, symmetric root-Q bell curves, and phase-compensated Mid/Side/Left/Right band placement.
- **Smooth Dynamics & Output:** Added primed equal-power topology crossfades, S-curve bypass fades, auto-gain compensation, and antialiased asymmetric soft saturation at the output ceiling.
