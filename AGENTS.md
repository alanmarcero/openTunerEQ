# openTunerEQ

C++20 / JUCE 8.0.9 audio plugin. See docs/design.md for behavior.

- Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build --config Release --parallel 3`.
- Test: `ctest --test-dir build -C Release --output-on-failure`.
- Core-only: configure with `-DOTE_BUILD_PLUGIN=OFF` in build-core.
- Audio thread: no allocation, locking, logging, UI access, or parameter ID construction.
- Note snapping and frequency bounds must agree between DSP, graph and all note controls.
- Keep generated art embedded through TunerAssets; render text and controls live.
- Verify DSP changes at low and high sample rates and under automation. Use pluginval before releases.
