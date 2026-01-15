## Kokoro TTS Helper - GDScript wrapper for TextToSpeech node
## Usage:
##   var tts = KokoroTTS.new()
##   add_child(tts)
##   await tts.initialize()
##   var audio = tts.speak("Hello world!")  # Blocking
##   # OR use async (non-blocking):
##   tts.speak_async("Hello world!")
##   # Then handle the generation_completed signal
##   $AudioStreamPlayer.stream = audio
##   $AudioStreamPlayer.play()

class_name KokoroTTS
extends Node

signal initialized()
signal speech_ready(audio: AudioStreamWAV)
signal generation_started(request_id: int)
signal generation_completed(request_id: int, audio: AudioStreamWAV)
signal generation_failed(request_id: int, error: String)
signal chunk_ready(request_id: int, chunk_index: int, total_chunks: int, audio: AudioStreamWAV)
signal stream_completed(request_id: int)

## Path to the Kokoro model files
@export_group("Model")
@export var model_path: String = "res://addons/godot_kokoro/models/model.onnx"
@export var voices_path: String = "res://addons/godot_kokoro/models/voices_anime.bin"
@export var tokens_path: String = "res://addons/godot_kokoro/models/tokens.txt"
@export var data_dir: String = "res://addons/godot_kokoro/models/espeak-ng-data"

## Multi-language model settings (for kokoro v1.0+)
@export_group("Multi-Language")
## Lexicon file for multi-lang models (e.g., lexicon-us-en.txt)
@export var lexicon_path: String = "res://addons/godot_kokoro/models/lexicon-us-en.txt"
## Dictionary directory for multi-lang models
@export var dict_dir: String = "res://addons/godot_kokoro/models/dict"
## Language code (e.g., "en-us", "zh", "ja") - leave empty for English-only models
@export var lang: String = "en-us"

## Speaker ID (0 to speaker_count-1)
@export_group("Voice")
@export var speaker_id: int = 0:
	set(value):
		speaker_id = value
		if _tts:
			_tts.speaker_id = value

## Speech speed (0.5 = slow, 1.0 = normal, 2.0 = fast)
@export_range(0.5, 2.0, 0.1) var speed: float = 1.0:
	set(value):
		speed = value
		if _tts:
			_tts.speed = value

## Performance Settings
@export_group("Performance")

## Number of CPU threads (0 = auto-detect optimal count)
@export_range(0, 16, 1) var num_threads: int = 0:
	set(value):
		num_threads = value
		if _tts:
			_tts.num_threads = value

## Enable debug output (disable for better performance)
@export var debug_mode: bool = false:
	set(value):
		debug_mode = value
		if _tts:
			_tts.debug_mode = value

## Max sentences per batch (higher = better for long text, lower = lower latency)
@export_range(1, 10, 1) var max_sentences: int = 2:
	set(value):
		max_sentences = value
		if _tts:
			_tts.max_sentences = value

## Streaming Settings
@export_group("Streaming")

## Enable filler words for near-instant response (e.g., "Hmm,", "Well,")
@export var use_filler_words: bool = false

## Available filler words to prepend (randomly selected)
@export var filler_words: PackedStringArray = [
  "Hmm.", "Hmmm.", "Huh.", "Uh.", "Um.", "Umm.", "Erm.", "Err.", "Eh.",
  "Aha.", "Oh!", "Ohh.", "Ooh!", "Ah!", "Aah!", "Wow.", "Whoa.",
  "Uh-oh.", "Ehh.", "Yikes.", "Heh…","Yeah."
]

var _tts: TextToSpeech = null
var _audio_queue: Array[AudioStreamWAV] = []
var _is_streaming: bool = false
var _current_stream_id: int = 0

func _ready():
	# Create TextToSpeech node
	_tts = TextToSpeech.new()
	_tts.name = "TextToSpeech"
	add_child(_tts)

	# Apply performance settings before model loads
	_tts.num_threads = num_threads
	_tts.debug_mode = debug_mode
	_tts.max_sentences = max_sentences

	# Connect signals
	_tts.model_loaded.connect(_on_model_loaded)
	_tts.speech_generated.connect(_on_speech_generated)
	_tts.generation_started.connect(_on_generation_started)
	_tts.generation_completed.connect(_on_generation_completed)
	_tts.generation_failed.connect(_on_generation_failed)
	_tts.chunk_ready.connect(_on_chunk_ready)
	_tts.stream_completed.connect(_on_stream_completed)

## Initialize the TTS engine with the configured model
func initialize() -> bool:
	if not _tts:
		print("KokoroTTS ERROR: TextToSpeech node not created")
		return false

	# Check if files exist (with debug output)
	var model_exists = FileAccess.file_exists(model_path)
	var voices_exists = FileAccess.file_exists(voices_path)
	var tokens_exists = FileAccess.file_exists(tokens_path)
	var data_exists = data_dir.is_empty() or DirAccess.dir_exists_absolute(data_dir)

	print("KokoroTTS checking files:")
	print("  model_path: ", model_path, " -> exists: ", model_exists)
	print("  voices_path: ", voices_path, " -> exists: ", voices_exists)
	print("  tokens_path: ", tokens_path, " -> exists: ", tokens_exists)
	print("  data_dir: ", data_dir, " -> exists: ", data_exists)

	if not model_exists:
		print("KokoroTTS ERROR: Model file not found: " + model_path)
		return false
	if not voices_exists:
		print("KokoroTTS ERROR: Voices file not found: " + voices_path)
		return false
	if not tokens_exists:
		print("KokoroTTS ERROR: Tokens file not found: " + tokens_path)
		return false
	if not data_exists:
		print("KokoroTTS ERROR: Data directory not found: " + data_dir)
		return false

	print("KokoroTTS: Loading model...")
	_tts.load_model(model_path, voices_path, tokens_path, data_dir, lexicon_path, dict_dir, lang)
	_tts.speaker_id = speaker_id
	_tts.speed = speed

	var loaded = _tts.is_model_loaded()
	print("KokoroTTS: Model loaded: ", loaded)
	return loaded

## Check if model is loaded
func is_ready() -> bool:
	return _tts and _tts.is_model_loaded()

## Generate speech from text
func speak(text: String) -> AudioStreamWAV:
	if not is_ready():
		push_error("KokoroTTS: Model not loaded")
		return null
	return _tts.speak(text)

## Get the number of available speakers/voices
func get_speaker_count() -> int:
	if not _tts:
		return 0
	return _tts.get_speaker_count()

## Get the audio sample rate
func get_sample_rate() -> int:
	if not _tts:
		return 0
	return _tts.get_sample_rate()

func _on_model_loaded():
	initialized.emit()

func _on_speech_generated(audio: AudioStreamWAV):
	speech_ready.emit(audio)

func _on_generation_started(request_id: int):
	generation_started.emit(request_id)

func _on_generation_completed(request_id: int, audio: AudioStreamWAV):
	generation_completed.emit(request_id, audio)

func _on_generation_failed(request_id: int, error: String):
	generation_failed.emit(request_id, error)

func _on_chunk_ready(request_id: int, chunk_index: int, total_chunks: int, audio: AudioStreamWAV):
	chunk_ready.emit(request_id, chunk_index, total_chunks, audio)

func _on_stream_completed(request_id: int):
	_is_streaming = false
	stream_completed.emit(request_id)

## Async speech generation (non-blocking) - returns request ID
func speak_async(text: String) -> int:
	if not is_ready():
		push_error("KokoroTTS: Model not loaded")
		return 0
	return _tts.speak_async(text)

## Streaming speech generation (low-latency chunked) - returns request ID
## Audio is generated in chunks and chunk_ready signal is emitted for each chunk
## Use this for lowest perceived latency - first audio plays in ~0.3s instead of ~1.3s
func speak_streaming(text: String) -> int:
	if not is_ready():
		push_error("KokoroTTS: Model not loaded")
		return 0

	_audio_queue.clear()
	_is_streaming = true

	# Optionally prepend a filler word for near-instant response
	var full_text = text
	if use_filler_words and filler_words.size() > 0:
		var filler = filler_words[randi() % filler_words.size()]
		full_text = filler + " " + text

	_current_stream_id = _tts.speak_streaming(full_text)
	return _current_stream_id

## Check if currently streaming
func is_streaming() -> bool:
	return _is_streaming

## Check if TTS is currently generating audio
func is_generating() -> bool:
	if not _tts:
		return false
	return _tts.is_generating()

## Cancel pending async generation requests
func cancel() -> void:
	if _tts:
		_tts.cancel_generation()

## Get the optimal thread count for this system
func get_optimal_thread_count() -> int:
	if _tts:
		return _tts.get_optimal_thread_count()
	# Fallback: estimate based on processor count
	var cpu_count = OS.get_processor_count()
	if cpu_count <= 2:
		return 1
	elif cpu_count <= 4:
		return cpu_count - 1
	elif cpu_count <= 8:
		return cpu_count - 2
	else:
		return 8
