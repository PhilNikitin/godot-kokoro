# Godot Kokoro TTS - Build Instructions

> **Platform support:** [Windows](#windows-build-instructions) | [macOS](#macos-build-instructions-apple-silicon--universal)

---

## Windows Build Instructions

### Prerequisites

1. **Python 3.x** with SCons (`pip install scons`)
2. **Visual Studio 2022** with C++ desktop development workload
3. **Git** for cloning repositories

## Step 1: Download sherpa-onnx Libraries

Download the pre-built Windows x64 shared libraries:

```powershell
# Option A: From HuggingFace (recommended)
# Visit: https://huggingface.co/csukuangfj/sherpa-onnx-libs/tree/main
# Download: sherpa-onnx-v1.12.20-win-x64-shared.tar.bz2

# Option B: From GitHub Releases
# Visit: https://github.com/k2-fsa/sherpa-onnx/releases
# Look for: sherpa-onnx-v1.12.x-win-x64-shared.tar.bz2
```

Extract and copy to the project:
```
sherpa-onnx/
├── include/
│   └── sherpa-onnx/
│       └── c-api/
│           └── c-api.h
└── lib/
    ├── sherpa-onnx-c-api.dll
    ├── sherpa-onnx-c-api.lib
    ├── onnxruntime.dll
    └── onnxruntime.lib
```

## Step 2: Download Kokoro Model

The models are in a special **tts-models** release tag (not regular releases):

```powershell
# Option A: English only (11 speakers, ~330MB)
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2

# Option B: Multi-language EN+ZH (53 speakers, ~310MB)
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-multi-lang-v1_0.tar.bz2

# Option C: Multi-language EN+ZH int8 quantized (103 speakers, smaller)
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-int8-multi-lang-v1_1.tar.bz2
```

Direct download links:
- https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2
- https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-multi-lang-v1_0.tar.bz2

Extract to: `addons/godot_kokoro/models/`
Should contain:
- `model.onnx`
- `voices.bin`
- `tokens.txt`

## Step 3: Clone godot-cpp

```powershell
cd godot_kokoro
git clone https://github.com/godotengine/godot-cpp --branch godot-4.3-stable --depth 1
```

## Step 4: Build godot-cpp

Open **Developer Command Prompt for VS 2022**:

```powershell
cd godot_kokoro/godot-cpp
scons platform=windows target=template_debug
scons platform=windows target=template_release
```

## Step 5: Build the Extension

```powershell
cd godot_kokoro
scons platform=windows target=template_debug
scons platform=windows target=template_release
```

## Step 6: Copy Files to Addon

Copy the built files to the addon folder:

```powershell
# Copy extension DLL
copy bin\godot_kokoro.windows.template_debug.x86_64.dll ..\addons\godot_kokoro\bin\
copy bin\godot_kokoro.windows.template_release.x86_64.dll ..\addons\godot_kokoro\bin\

# Copy sherpa-onnx DLLs
copy sherpa-onnx\lib\sherpa-onnx-c-api.dll ..\addons\godot_kokoro\bin\
copy sherpa-onnx\lib\onnxruntime.dll ..\addons\godot_kokoro\bin\

# Copy any other required DLLs (check sherpa-onnx lib folder)
```

## Step 7: Test in Godot

1. Open your Godot project
2. Create a new scene with:
   - TextToSpeech node
   - AudioStreamPlayer node

3. Add script:
```gdscript
extends Node

@onready var tts: TextToSpeech = $TextToSpeech
@onready var player: AudioStreamPlayer = $AudioStreamPlayer

func _ready():
    tts.load_model(
        "res://addons/godot_kokoro/models/model.onnx",
        "res://addons/godot_kokoro/models/voices.bin",
        "res://addons/godot_kokoro/models/tokens.txt"
    )

    var audio = tts.speak("Hello, this is a test!")
    player.stream = audio
    player.play()
```

## Troubleshooting

### DLL not found
- Ensure all DLLs are in `addons/godot_kokoro/bin/`
- Check for missing dependencies with Dependency Walker

### Model load fails
- Verify model files exist at the specified paths
- Check console for error messages
- Ensure paths are correct (use `res://` prefix)

### No audio output
- Check if AudioStreamPlayer is configured correctly
- Verify sample rate matches (Kokoro uses 24000 Hz)

## File Structure

After building, your addon should look like:
```
addons/godot_kokoro/
├── godot_kokoro.gdextension
├── kokoro_tts.gd
├── bin/
│   ├── godot_kokoro.windows.template_debug.x86_64.dll
│   ├── godot_kokoro.windows.template_release.x86_64.dll
│   ├── sherpa-onnx-c-api.dll
│   └── onnxruntime.dll
└── models/
    ├── model.onnx
    ├── voices.bin
    └── tokens.txt
```

---

## macOS Build Instructions (Apple Silicon / Universal)

The output is a **universal2** framework (arm64 + x86_64) that runs natively on Apple Silicon (M1/M2/M3/M4) and Intel Macs. The C++ source requires no changes — only the build toolchain and libraries differ from Windows.

### Prerequisites

1. **Xcode Command Line Tools**: `xcode-select --install`
2. **Python 3.x** with SCons: `pip3 install scons`
3. **Git**

### Step 1: Download sherpa-onnx macOS Libraries

Download the pre-built macOS universal2 shared libraries from the sherpa-onnx GitHub releases:

```bash
# Visit: https://github.com/k2-fsa/sherpa-onnx/releases/latest
# Download the file named: sherpa-onnx-vX.X.X-osx-universal2-shared.tar.bz2

curl -LO https://github.com/k2-fsa/sherpa-onnx/releases/download/v1.12.25/sherpa-onnx-v1.12.25-osx-universal2-shared.tar.bz2
tar xjf sherpa-onnx-v1.12.25-osx-universal2-shared.tar.bz2
```

Create the sherpa-onnx directory structure inside `godot_kokoro/`:

```bash
mkdir -p godot_kokoro/sherpa-onnx/include/sherpa-onnx/c-api
mkdir -p godot_kokoro/sherpa-onnx/lib

# Copy header
cp sherpa-onnx-v1.12.25-osx-universal2-shared/include/sherpa-onnx/c-api/c-api.h \
   godot_kokoro/sherpa-onnx/include/sherpa-onnx/c-api/

# Copy libraries — use -a to preserve symlinks
cp -a sherpa-onnx-v1.12.25-osx-universal2-shared/lib/libsherpa-onnx-c-api.dylib \
      godot_kokoro/sherpa-onnx/lib/
cp -a sherpa-onnx-v1.12.25-osx-universal2-shared/lib/libonnxruntime*.dylib \
      godot_kokoro/sherpa-onnx/lib/
```

Expected structure (ONNX Runtime version number may differ):
```
godot_kokoro/sherpa-onnx/
├── include/
│   └── sherpa-onnx/
│       └── c-api/
│           └── c-api.h
└── lib/
    ├── libsherpa-onnx-c-api.dylib
    ├── libonnxruntime.dylib          ← symlink -> libonnxruntime.1.x.x.dylib
    └── libonnxruntime.1.x.x.dylib   ← actual file
```

> **Note:** Unlike Windows, macOS does NOT need `onnxruntime_providers_shared`.

### Step 2: Download Kokoro Model

Same models as Windows — see [Step 2 above](#step-2-download-kokoro-model). Extract to `addons/godot_kokoro/models/`.

### Step 3: Clone godot-cpp

```bash
cd godot_kokoro
git clone https://github.com/godotengine/godot-cpp --branch godot-4.3-stable --depth 1
```

### Step 4: Build godot-cpp for macOS

```bash
cd godot_kokoro/godot-cpp
scons platform=macos target=template_debug
scons platform=macos target=template_release
```

This builds universal2 binaries by default (`arch=universal`).

### Step 5: Build the Extension

```bash
cd godot_kokoro
scons platform=macos target=template_debug
scons platform=macos target=template_release
```

Output:
```
godot_kokoro/bin/
├── libgodot_kokoro.macos.template_debug.framework/
│   └── libgodot_kokoro.macos.template_debug
└── libgodot_kokoro.macos.template_release.framework/
    └── libgodot_kokoro.macos.template_release
```

Verify the universal2 binary:
```bash
lipo -info bin/libgodot_kokoro.macos.template_debug.framework/libgodot_kokoro.macos.template_debug
# Expected: Architectures in the fat file: ... are: x86_64 arm64
```

### Step 6: Verify Library Dependencies

Check that the RPATH is embedded correctly:
```bash
otool -l bin/libgodot_kokoro.macos.template_debug.framework/libgodot_kokoro.macos.template_debug | grep -A2 LC_RPATH
# Must show: path @loader_path/..
```

Check that sherpa-onnx uses `@rpath`-relative install names (not absolute paths):
```bash
otool -L sherpa-onnx/lib/libsherpa-onnx-c-api.dylib
# Should show @rpath/libonnxruntime... — NOT an absolute /path/to/...
```

If absolute paths appear, fix with `install_name_tool`:
```bash
install_name_tool -change /absolute/path/to/libonnxruntime.dylib \
    @rpath/libonnxruntime.dylib \
    sherpa-onnx/lib/libsherpa-onnx-c-api.dylib
```

### Step 7: Copy Files to Addon

```bash
# Copy framework directories
cp -r bin/libgodot_kokoro.macos.template_debug.framework \
      ../addons/godot_kokoro/bin/
cp -r bin/libgodot_kokoro.macos.template_release.framework \
      ../addons/godot_kokoro/bin/

# Copy sherpa-onnx dylibs — use -a to preserve symlinks
cp -a sherpa-onnx/lib/libsherpa-onnx-c-api.dylib \
      ../addons/godot_kokoro/bin/
cp -a sherpa-onnx/lib/libonnxruntime*.dylib \
      ../addons/godot_kokoro/bin/
```

### Step 8: Ad-hoc Code Sign (Required)

macOS blocks unsigned binaries loaded as plugins. Ad-hoc signing is sufficient for development:

```bash
cd ../addons/godot_kokoro/bin

# Sign dylibs
codesign --force --sign - libsherpa-onnx-c-api.dylib
codesign --force --sign - libonnxruntime.1.23.2.dylib
# libonnxruntime.dylib is a copy included for linking convenience — sign it too if present:
# codesign --force --sign - libonnxruntime.dylib

# Sign framework binaries
codesign --force --sign - \
    "libgodot_kokoro.macos.template_debug.framework/libgodot_kokoro.macos.template_debug"
codesign --force --sign - \
    "libgodot_kokoro.macos.template_release.framework/libgodot_kokoro.macos.template_release"
```

Also clear any quarantine flags set by macOS on downloaded files:
```bash
xattr -r -d com.apple.quarantine .
```

### Step 9: Test in Godot

Open the project in Godot on macOS and run the `tts_test.tscn` scene, or use the same test script as Windows (see [Step 7 above](#step-7-test-in-godot)).

### Final File Structure (macOS)

```
addons/godot_kokoro/bin/
├── libgodot_kokoro.macos.template_debug.framework/
│   └── libgodot_kokoro.macos.template_debug
├── libgodot_kokoro.macos.template_release.framework/
│   └── libgodot_kokoro.macos.template_release
├── libsherpa-onnx-c-api.dylib
├── libonnxruntime.dylib              ← symlink (must be present!)
└── libonnxruntime.1.x.x.dylib        ← actual versioned file
```

### Troubleshooting (macOS)

**Crash or "dylib not found" at startup**
- Verify RPATH: `otool -l .../libgodot_kokoro.macos.template_debug | grep -A2 LC_RPATH` — must show `@loader_path/..`
- Ensure dylibs are directly in `addons/godot_kokoro/bin/` (not in a subdirectory)
- Confirm both the symlink and the versioned `.dylib` file are present

**Gatekeeper blocks loading / "killed" process**
- Run the `codesign` step (Step 8) on all dylibs and framework binaries
- Run `xattr -r -d com.apple.quarantine addons/godot_kokoro/bin/`

**Architecture mismatch**
- Verify with `lipo -info` that the framework binary is universal2 (arm64 + x86_64)

**Model load fails**
- Same as Windows — verify model file paths use `res://` prefix and files exist
