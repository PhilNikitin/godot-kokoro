# Godot Kokoro TTS - Build Instructions

## Prerequisites

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
