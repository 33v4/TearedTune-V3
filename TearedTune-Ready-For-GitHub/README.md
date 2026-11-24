# TearedTune V3

TearedTune V3 is an original Windows x64 VST3 pitch-correction effect built with JUCE 8. It supports key and scale selection, retune speed, vibrato preservation, note selection, input/output controls, and a live pitch display.

## Build and download from GitHub

1. Create a new GitHub repository. Leave it empty when creating it; do not add a GitHub README or `.gitignore` yet.
2. Extract this project ZIP.
3. Upload the **contents** of the extracted TearedTune folder into the repository root. Do not upload the outer folder as a subfolder. In the repository root, `CMakeLists.txt`, `Source`, `installer`, and `.github` should be visible together.
4. Commit the files to the `main` branch.
5. Open the repository's **Actions** tab and wait for **Build TearedTune V3 VST3 (Windows x64)** to finish.
6. Open the successful run and download the `TearedTune-V3-VST3-Install` artifact. GitHub downloads it as a ZIP containing `TearedTune V3.vst3` and the install guide.
7. Extract the ZIP and copy the entire `TearedTune V3.vst3` folder to `C:\Program Files\Common Files\VST3`.
8. In FL Studio, rescan plugins and add TearedTune V3 to a mixer insert as an effect.

The GitHub workflow is at `.github/workflows/build.yml`. It builds with the Visual Studio 2022 toolchain on a Windows 2022 runner. JUCE 8.0.4 is fetched by CMake. The downloaded Actions artifact is retained for 30 days.

## Project layout

```text
CMakeLists.txt
.github/workflows/build.yml
Source/
installer/README_INSTALL.txt
tests/dsp_test.cpp
docs/
tools/
```

## Notes

The DSP self-test is an optional target and is not part of the standard plugin build. The test report describes the offline DSP checks; the JUCE plugin build and FL Studio host scan still need to be confirmed by the GitHub Actions run and a local DAW test.

This project uses JUCE under its licensing terms. Review the JUCE license before distributing or selling builds.
