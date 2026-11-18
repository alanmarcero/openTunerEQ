# openTunerEQ

A five-band equalizer with note-aware controls and a live spectrum display.

![openTunerEQ equalizer and analyzer](docs/screenshots/preview.png)

[Download the latest release](https://github.com/alanmarcero/openTunerEQ/releases/latest) · [Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

The tuning fork in the header pulses on host quarter notes and holds when the host stops. In standalone mode it follows a 120 BPM sample clock.

## Equalizer

Each band has its own enable switch, frequency, gain, Q, and filter shape. Choose Bell, Low shelf, High shelf, High pass, Low pass, Notch, or Band pass. Frequency covers 20 Hz to 20 kHz, with a lower maximum at low sample rates. Gain covers ±24 dB, and Q covers 0.1 to 18. High-pass and low-pass slopes are 12, 24, 48, or 96 dB per octave.

Turn on **Snap to notes** to tune bands to semitones, including host automation. Set the A4 reference from 400 to 480 Hz to match your tuning. Choose a note and octave, or step one semitone at a time. Display notes as sharps or flats. Turn snapping off for free tuning; the display still shows the nearest note and cents.

Drag a band on the graph to change frequency and gain. Scroll over it to change Q. Double-click a band to bypass it. Hover to read frequency and note. With the analyzer on, the graph also shows audio level. Alt/Option-click to move the selected band to that frequency; turn snapping off for exact placement.

## Spectrum display

Choose Spectrum, Bars, or Spectrogram. View audio before or after the EQ, freeze peaks, set Slow, Medium, or Fast falloff, and tilt the spectrum by 0, 3, 4.5, or 6 dB per octave. The editor saves these display settings with your session.

Output trim ranges from −24 to +12 dB. Global bypass includes output trim. The EQ supports mono and stereo and adds no latency.

## Formats

- **macOS 11 or newer:** Universal VST3, Audio Unit (AU), and standalone app.
- **Windows x64:** VST3 and standalone app.

Use [OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest) or download a package from the [latest release](https://github.com/alanmarcero/openTunerEQ/releases/latest).

## License

AGPL-3.0. See [LICENSE](LICENSE) for terms.
