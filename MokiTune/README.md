# MokiTune — real-time vocal pitch correction (prototype)

MokiTune is a JUCE-based VST3/AU audio plug-in prototype for real-time, monophonic vocal pitch correction. It includes a custom interface and host-automatable parameters.

## Controls

- **Key** — musical root note.
- **Scale** — Chromatic, Major, or Minor.
- **Correction** — amount of movement toward the nearest allowed note (0–100%).
- **Retune Speed** — smoothing time from 1 to 100 ms. Lower values sound more immediate.
- **Mix** — dry/wet blend.
- **Bypass** — outputs the latency-aligned dry signal.

The display reports the estimated input pitch. The plug-in currently targets one lead vocal at a time.

## Current limitations

This is a starter implementation, not a finished commercial Auto-Tune replacement. Pitch detection uses a YIN-style difference function; pitch shifting uses a crossfaded variable-delay method. It can create audible artifacts, especially with large pitch moves, breathy/noisy sources, vibrato, or overlapping voices. It does **not** preserve formants, and its tuning/scale logic is intentionally simple. Use it as a foundation for listening tests and further development rather than assuming transparent studio-grade correction.

The processing path does not allocate memory inside `processBlock`, and parameters are exposed to the host for automation. Reported latency is approximately 20 ms, plus any buffering/latency introduced by the DAW and audio interface.

## Compile online (recommended for Apple Silicon)

This repository includes a GitHub Actions workflow at `.github/workflows/build-apple-silicon.yml`. It builds arm64 versions of both VST3 and AU on a GitHub-hosted macOS runner and uploads them as downloadable ZIP artifacts.

1. Create a GitHub account and a **new repository** (for example, `MokiTune`).
2. Upload all files and folders from this project, including the hidden `.github` folder and its workflow file. The easiest way is to extract the project ZIP locally and upload the files to the repository; make sure `.github/workflows/build-apple-silicon.yml` is included.
3. In the repository, open **Actions** and enable workflows if GitHub asks.
4. Select **Build MokiTune for Apple Silicon** and click **Run workflow**. Alternatively, pushing to the `main` or `master` branch starts a build automatically.
5. When the run finishes successfully, open the run page and download the artifact named **MokiTune-Apple-Silicon-Plugins**. It contains separate ZIPs for VST3 and AU.
6. On your Mac, unzip the VST3 archive and copy `MokiTune.vst3` to `~/Library/Audio/Plug-Ins/VST3/`. For AU, copy `MokiTune.component` to `~/Library/Audio/Plug-Ins/Components/`. Restart your DAW or rescan plug-ins.

The workflow pins JUCE to version 8.0.6 for reproducibility and targets `arm64` with a macOS 11.0 deployment target. GitHub-hosted build artifacts are temporary and are retained for 14 days by this workflow. This produces a build; it does not sign/notarize the plugin for distribution to other users.

## Build on macOS (Apple Silicon or Intel)

Requirements: Xcode Command Line Tools, CMake 3.22+, Git, and a recent JUCE checkout. JUCE is a separate dependency and is not bundled in this source archive.

```bash
xcode-select --install
brew install cmake git
mkdir -p "$HOME/Developer"
git clone https://github.com/juce-framework/JUCE.git "$HOME/Developer/JUCE"
```

From the directory containing this README:

```bash
cmake -S . -B build -DJUCE_DIR="$HOME/Developer/JUCE" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
```

Expected plug-in bundles are created under:

```text
build/MokiTune_artefacts/Release/VST3/MokiTune.vst3
build/MokiTune_artefacts/Release/AU/MokiTune.component
```

To install the VST3 for your user account:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
cp -R "build/MokiTune_artefacts/Release/VST3/MokiTune.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/"
```

For an AU host such as Logic Pro, copy the `.component` bundle to `~/Library/Audio/Plug-Ins/Components/`. Restart or rescan plug-ins in your DAW. On macOS, the first build may take a while because JUCE and the plug-in formats need to compile.

If CMake does not find JUCE, confirm the path passed to `-DJUCE_DIR` contains JUCE's top-level `CMakeLists.txt`.

## Licensing note

JUCE is distributed under its own licensing terms (including GPL and commercial licensing options). Check the current JUCE license and make sure the chosen license fits how you plan to use or distribute this plug-in. This project source itself is supplied as a prototype for evaluation and modification.
