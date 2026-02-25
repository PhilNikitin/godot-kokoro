# Godot Kokoro TTS

A Godot 4.1+ GDExtension for high-quality text-to-speech using the Kokoro TTS engine (via Sherpa-ONNX). Powers [Project Alex](https://store.steampowered.com/app/4342230/Project_Alex) AI desktop companion — **[demo out now on Steam!](https://store.steampowered.com/app/4342230/Project_Alex)**

[![Project Alex](https://img.itch.zone/aW1nLzI1MDA5Mjg4LnBuZw==/315x250%23c/IGqJE3.png)](https://store.steampowered.com/app/4342230/Project_Alex)

## Features

- High-quality neural TTS with multiple voices
- Multi-language support (English, Chinese, Japanese)
- Async and streaming generation modes
- Low-latency streaming for real-time applications

## Installation

1. Copy the `addons/godot_kokoro` folder to your project's `addons/` directory

2. Download the Kokoro model files (choose one):

   **Multi-language (EN+ZH, 53 speakers, recommended):**
   ```
   https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-multi-lang-v1_0.tar.bz2
   ```

   **English only (11 speakers, smaller):**
   ```
   https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2
   ```

   **Int8 quantized (103 speakers, smallest):**
   ```
   https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-int8-multi-lang-v1_1.tar.bz2
   ```

3. Extract model files to `addons/godot_kokoro/models/`:
   ```
   addons/godot_kokoro/models/
   ├── model.onnx
   ├── voices.bin
   └── tokens.txt
   ```

## Quick Start

```gdscript
extends Node

var tts: KokoroTTS

func _ready():
    tts = KokoroTTS.new()
    add_child(tts)

    if tts.initialize():
        print("TTS ready! %d speakers available" % tts.get_speaker_count())

    # Generate and play speech
    var audio = tts.speak("Hello, world!")
    $AudioStreamPlayer.stream = audio
    $AudioStreamPlayer.play()
```

### Async Mode (non-blocking)

```gdscript
func _ready():
    tts = KokoroTTS.new()
    add_child(tts)
    tts.initialize()

    tts.generation_completed.connect(_on_speech_ready)
    tts.speak_async("Hello, this won't block!")

func _on_speech_ready(request_id: int, audio: AudioStreamWAV):
    $AudioStreamPlayer.stream = audio
    $AudioStreamPlayer.play()
```

### Streaming Mode (lowest latency)

```gdscript
func _ready():
    tts = KokoroTTS.new()
    add_child(tts)
    tts.initialize()

    tts.chunk_ready.connect(_on_chunk_ready)
    tts.speak_streaming("This plays as it generates!")

func _on_chunk_ready(request_id: int, chunk_index: int, total: int, audio: AudioStreamWAV):
    if chunk_index == 0:
        $AudioStreamPlayer.stream = audio
        $AudioStreamPlayer.play()
```

## Configuration

```gdscript
tts.speaker_id = 5      # Voice selection (0 to speaker_count-1)
tts.speed = 1.2         # Speech speed (0.5 to 2.0)
tts.lang = "en-us"      # Language: "en-us", "zh", "ja"
```

## Building from Source

See [godot_kokoro/BUILD_INSTRUCTIONS.md](godot_kokoro/BUILD_INSTRUCTIONS.md) for build instructions.

## Projects Using This

- **[Project Alex](https://store.steampowered.com/app/4342230/Project_Alex)** - AI-powered desktop companion with voice interaction ([itch.io](https://berilli.itch.io/project-alex) | [Steam — demo out now!](https://store.steampowered.com/app/4342230/Project_Alex))

## License

This project is licensed under the MIT License.

### Third-Party Licenses

- **Kokoro TTS Models** - Apache 2.0 License ([Kokoro](https://github.com/hexgrad/kokoro))
- **Sherpa-ONNX** - Apache 2.0 License ([k2-fsa/sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx))
- **ONNX Runtime** - MIT License ([Microsoft](https://github.com/microsoft/onnxruntime))
