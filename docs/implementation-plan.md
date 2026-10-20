# openTunerEQ implementation plan

Goal: ship a musical five-band EQ with native UI and a working GitHub pipeline.
Architecture: independent DSP core, JUCE processor/state layer, native UI.
Spec: design.md. Execution: inline, in this new isolated repository.

## 1. Core
- Write behavior tests in Tests/CoreTests.cpp for A4/C4, boundary snapping, spelling and cents; cutoff -3 dB and octave attenuation; gain/notch/shelf; bypass, stereo independence and finite output under parameter changes.
- Implement Source/Core/{Notes,Filters,Equalizer}.h/.cpp with fixed storage and double filter states.
- Configure with `cmake -S . -B build-core -DOTE_BUILD_PLUGIN=OFF`; run CTest. No audio-thread allocation.

## 2. Plugin and interface
- Create Source/Plugin/{Parameters,PluginProcessor,PluginEditor,Theme,BandPanel,ResponseGraph}.h/.cpp.
- Parameters carry raw Hz and are snapped at the DSP boundary; UI displays that same effective frequency. Use parameter attachments and host gestures.
- Add processor integration tests for state, mono/stereo, bypass, automation and editor snapshot. Embed generated assets via juce_add_binary_data.
- Build VST3, AU and Standalone, run CTest and pluginval.

## 3. Delivery
- Add macOS universal and Windows CI jobs with tests, pluginval and archive artifacts, plus release job on version tags.
- README documents controls, filter slope vs attenuation, local build, install and licensing. Include an actual editor screenshot.
- Review code, verify local results, create public GitHub repo and push. Observe CI and correct failures.

## Review focus
- Near-Nyquist behavior and low-frequency high-order stability.
- Rapid shape/slope automation during crossfades.
- State restore synchronizes DSP and note controls.
- Gain, Q, and slope controls are visibly inactive when they have no effect.
- Every UI frequency path uses the same snapping rule; invalid piano notes cannot be selected.
