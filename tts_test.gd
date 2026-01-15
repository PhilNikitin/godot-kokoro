extends Node

@onready var player: AudioStreamPlayer = $AudioStreamPlayer
@onready var text_input: LineEdit = $UI/VBoxContainer/LineEdit
@onready var speak_button: Button = $UI/VBoxContainer/SpeakButton
@onready var status_label: Label = $UI/VBoxContainer/StatusLabel
@onready var speaker_slider: HSlider = $UI/VBoxContainer/SpeakerContainer/SpeakerSlider
@onready var speaker_label: Label = $UI/VBoxContainer/SpeakerContainer/SpeakerLabel
@onready var speed_slider: HSlider = $UI/VBoxContainer/SpeedContainer/SpeedSlider
@onready var speed_label: Label = $UI/VBoxContainer/SpeedContainer/SpeedLabel

var tts: KokoroTTS
var generation_start_time: int = 0
var first_chunk_time: int = 0

# Streaming mode
var use_streaming: bool = true  # Toggle this to test streaming vs async
var audio_queue: Array[AudioStreamWAV] = []
var is_playing_stream: bool = false
var chunks_received: int = 0
var total_chunks_expected: int = 0
var waiting_for_chunk: bool = false

func _ready():
	status_label.text = "Initializing TTS..."

	# Create TTS instance
	tts = KokoroTTS.new()
	tts.debug_mode = true  # Temporarily enable to see thread count
	add_child(tts)

	# Initialize TTS
	if tts.initialize():
		var speaker_count = tts.get_speaker_count()
		status_label.text = "TTS Ready! %d speakers available" % speaker_count
		speaker_slider.max_value = speaker_count - 1
		speak_button.disabled = false
	else:
		status_label.text = "Failed to initialize TTS. Check model files."
		speak_button.disabled = true

	# Connect signals
	speak_button.pressed.connect(_on_speak_pressed)
	speaker_slider.value_changed.connect(_on_speaker_changed)
	speed_slider.value_changed.connect(_on_speed_changed)
	text_input.text_submitted.connect(_on_text_submitted)
	player.finished.connect(_on_audio_finished)

	# Connect async TTS signals
	tts.generation_completed.connect(_on_generation_completed)
	tts.generation_failed.connect(_on_generation_failed)

	# Connect streaming TTS signals
	tts.chunk_ready.connect(_on_chunk_ready)
	tts.stream_completed.connect(_on_stream_completed)

	# Set initial values
	_on_speaker_changed(speaker_slider.value)
	_on_speed_changed(speed_slider.value)

func _on_speak_pressed():
	if not tts.is_ready():
		status_label.text = "TTS not ready!"
		return

	var text = text_input.text.strip_edges()
	if text.is_empty():
		status_label.text = "Please enter some text"
		return

	status_label.text = "Generating speech..."
	speak_button.disabled = true

	# Record start time
	generation_start_time = Time.get_ticks_msec()
	first_chunk_time = 0

	if use_streaming:
		# Streaming mode: audio plays as chunks complete
		audio_queue.clear()
		is_playing_stream = false
		chunks_received = 0
		total_chunks_expected = 0
		waiting_for_chunk = false
		tts.speak_streaming(text)
	else:
		# Async mode: wait for full audio then play
		tts.speak_async(text)

func _on_generation_completed(_request_id: int, audio: AudioStreamWAV):
	var elapsed_ms = Time.get_ticks_msec() - generation_start_time
	var elapsed_sec = elapsed_ms / 1000.0
	status_label.text = "Playing (generated in %.2fs)..." % elapsed_sec
	player.stream = audio
	player.play()

func _on_generation_failed(_request_id: int, error: String):
	status_label.text = "Failed: " + error
	speak_button.disabled = false

func _on_audio_finished():
	if is_playing_stream:
		# Play next chunk in queue
		_play_next_chunk()
	else:
		status_label.text = "Ready"
		speak_button.disabled = false

func _on_chunk_ready(_request_id: int, chunk_index: int, total_chunks: int, audio: AudioStreamWAV):
	chunks_received += 1
	total_chunks_expected = total_chunks

	# Record time to first chunk
	if chunk_index == 0:
		first_chunk_time = Time.get_ticks_msec() - generation_start_time
		var first_sec = first_chunk_time / 1000.0
		status_label.text = "Playing chunk 1/%d (first in %.2fs)..." % [total_chunks, first_sec]

		# Start playing immediately
		is_playing_stream = true
		waiting_for_chunk = false
		player.stream = audio
		player.play()
	else:
		# Queue for later playback
		audio_queue.append(audio)
		status_label.text = "Chunk %d/%d ready..." % [chunk_index + 1, total_chunks]

		# If we were waiting for this chunk, play it now
		if waiting_for_chunk:
			waiting_for_chunk = false
			_play_next_chunk()

func _on_stream_completed(_request_id: int):
	var total_ms = Time.get_ticks_msec() - generation_start_time
	var total_sec = total_ms / 1000.0
	var first_sec = first_chunk_time / 1000.0
	print("Stream complete: first chunk in %.2fs, total %.2fs" % [first_sec, total_sec])

func _play_next_chunk():
	if audio_queue.is_empty():
		# Check if we're still expecting more chunks
		if chunks_received < total_chunks_expected:
			# Wait for next chunk to be generated
			waiting_for_chunk = true
			status_label.text = "Waiting for next chunk..."
		else:
			# All chunks played
			is_playing_stream = false
			var total_ms = Time.get_ticks_msec() - generation_start_time
			var total_sec = total_ms / 1000.0
			var first_sec = first_chunk_time / 1000.0
			status_label.text = "Done (first: %.2fs, total: %.2fs)" % [first_sec, total_sec]
			speak_button.disabled = false
	else:
		var next_audio = audio_queue.pop_front()
		player.stream = next_audio
		player.play()

func _on_speaker_changed(value: float):
	var id = int(value)
	tts.speaker_id = id
	speaker_label.text = "Speaker: %d" % id

func _on_speed_changed(value: float):
	tts.speed = value
	speed_label.text = "Speed: %.1f" % value

func _on_text_submitted(_text: String):
	_on_speak_pressed()
