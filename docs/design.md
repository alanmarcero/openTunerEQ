# openTunerEQ design

A five-band parametric EQ for placing filters at musical notes. User delegated interaction design and requested a new GitHub repository, CI, and generated UI imagery.

## Product

Five independently enabled bands with frequency (20–20,000 Hz), gain (±24 dB), Q (0.1–18), and Bell, Low shelf, High shelf, High pass, Low pass, Notch, Band pass shapes. HP/LP slopes: 12, 24, 48, 96 dB/oct. These are finite minimum-phase slopes, not an ideal brick wall. Shelf gain provides finite low/high attenuation in dB. Gain is inactive for cut/notch/band-pass shapes; Q affects bell/notch/band-pass and the resonance of 12 dB/oct cuts; higher-order cuts use a Butterworth alignment and Q is inactive. Shelf Q is fixed for monotonic shelves.

Each band displays effective frequency and chromatic note with octave. Global Snap defaults on; DSP and UI both honor it, including host automation. 12-tone equal temperament from an automatable A4 reference (400–480 Hz, default 440 Hz). Sharp/flat display is switchable. Direct pitch-class and octave selectors plus semitone step buttons and a clickable piano strip select notes. Free mode displays nearest note and cents. Out-of-range notes are disabled. Frequencies are bounded below Nyquist at every sample rate.

## Architecture

Pure C++20 core: note math, RBJ biquads, Butterworth cascades and five-band stereo processing. JUCE 8.0.9 integration: APVTS host parameters, preset state, mono/stereo buses, VST3/AU/Standalone. Frequency/gain/Q smoothing and bypass fades; shape/slope transitions crossfade two filter banks. No allocations or locks in processing. Filters use double-precision states, float audio buffers.

Native JUCE UI: embedded generated graphite faceplate and tuning-fork emblem, live response graph with draggable nodes, five band cards, piano strip, output trim and global bypass. Text remains drawn live and accessible, not baked into images. State recall and automation update the UI.

## Delivery and verification

New public repository alanmarcero/openTunerEQ, matching the public plugin family. CMake, unit tests independent of JUCE, processor/state/UI smoke test, macOS universal and Windows builds, pluginval at strictness 5, artifact ZIPs. Tagged releases publish packages. Local native build and pluginval before publication. Document unsigned distribution and installation. Source AGPL-3.0 to match the open-source JUCE licensing option.
