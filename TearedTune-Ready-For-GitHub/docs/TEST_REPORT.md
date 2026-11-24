# TearedTune V3 - test report

Date of test: 2026-11-24. Machine: Linux sandbox, g++ 13.3 (C++17), no GPU, no audio hardware, no Windows, no internet.

## 1. What was tested (offline DSP engine, `Source/PitchCorrector.h`)

Command: `g++ -O2 -std=c++17 -I Source tests/dsp_test.cpp -o dsp_test && ./dsp_test out/`
Results: **20 passed, 0 failed** at -O2 (`docs/test_output_O2.txt`).
Same suite built with AddressSanitizer + UndefinedBehaviorSanitizer: **19 passed, 0 failed, no memory or UB errors**
(`docs/test_output_asan.txt`; the CPU-speed check is skipped in the sanitizer build because timing is meaningless there).

Pitch is always measured with an **independent** autocorrelation estimator in the test, not the plugin's own detector.
Test signals are synthetic voice-like harmonic tones (24 harmonics, light noise), **not real recordings**.

| # | Test | Result |
|---|---|---|
| 1 | Pitch detector, 42 tones 82-880 Hz | worst error **1.00 cent**, 0 missed |
| 2 | Snap to a single allowed note (A4) from -40/-20/+30/+45 cents | output within **+-1.2 cents** of 440 Hz |
| 2b | C-major scale: Eb4 input | lands exactly on E4 (nearest scale note) |
| 3 | Retune speed (0 / 100 / 300 ms) | within 10 cents after 110 / 250 / 563 ms (includes ~30 ms detection latency) |
| 4 | Amount 0 / 50 % / bypass | pitch unchanged / halfway (+20.8 of +40 cents) / unchanged |
| 5 | Vibrato (+-40 ct @ 5.5 Hz): "Keep vibrato" 0 vs 1 | 0: residual depth **11.6 ct**; 1: depth **40.0 ct**, centre -0.2 ct |
| 6 | Artefacts on a pure sine corrected by 30 cents | energy outside the tone **-56 dB** |
| 7 | White noise / digital silence | level change -0.6 dB / output exactly 0 |
| 8 | Random parameters, random signals (noise, impulses, silence, clipping, NaN injection) at 22.05-192 kHz | 8.0 M samples, **0 non-finite**, bounded |
| 9 | CPU, stereo 44.1 kHz | ~**50x realtime** (~2% of one core) on this sandbox CPU |
| 10 | Demo melody, each note 20-45 cents off | median error vs intended note: **33.5 ct -> 4.7 ct** (hard tune) |

Before/after audio (left channel = original, right channel = corrected): `docs/audio/*.wav`.
I cannot listen to audio, so the numbers above are the evidence of correctness; **I have not judged the sound by ear.**

Bugs found: none in the engine; two test-code mistakes (a wrong power formula and an unrealistic vibrato threshold) were fixed and the suite re-run.

## 2. Known limitations measured/observed

* A detection-latency of roughly 15-30 ms means very fast vibrato (>5 Hz) is only partly flattened even at 0 ms retune (11.6 ct of 40 ct left).
* Resampling pitch shifter: formants move with the pitch (no formant preservation).
* Monophonic only; reverberant or layered vocals will confuse detection.
* Latency reporting is fixed (~8 ms). At Mix < 100 % the dry/wet alignment can differ by up to a few ms because the shifter's delay floats with the pitch ratio.

## 3. What was NOT tested

* **Compiling the JUCE shell (`PluginProcessor.*`, `PluginEditor.*`) and linking the VST3** - not possible here. Never compiled by me.
* **The GitHub Actions workflow** - never run. Its commands follow standard JUCE/CMake practice.
* **Loading in FL Studio or any DAW**, parameter automation, state save/restore, PDC behaviour.
* **The real user interface** (`docs/TearedTune_UI_preview.png` is a Python drawing of the intended design, not a screenshot).
* Listening tests on real voices, other sample rates in a host, CPU in a real host, stereo-linking behaviour on real material.
