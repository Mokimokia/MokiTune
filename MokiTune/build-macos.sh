#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JUCE_DIR="${JUCE_DIR:-$HOME/Developer/JUCE}"

if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
  echo "JUCE not found at: $JUCE_DIR"
  echo "Install JUCE first, or run: JUCE_DIR=/path/to/JUCE $0"
  echo "Download: https://github.com/juce-framework/JUCE"
  exit 1
fi

cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" \
  -DJUCE_DIR="$JUCE_DIR" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/build" --config Release --parallel 4

echo
echo "Build complete. Check these output paths:"
echo "  $ROOT_DIR/build/MokiTune_artefacts/Release/VST3/MokiTune.vst3"
echo "  $ROOT_DIR/build/MokiTune_artefacts/Release/AU/MokiTune.component"
