#include "text_to_speech.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>

// Include sherpa-onnx C API
#include "sherpa-onnx/c-api/c-api.h"

#include <cstring>
#include <algorithm>

using namespace godot;

int TextToSpeech::get_optimal_thread_count() {
    int cpu_count = static_cast<int>(std::thread::hardware_concurrency());
    if (cpu_count <= 0) cpu_count = 4;  // Fallback if detection fails

    // Strategy: Leave at least 1 core for main thread/game
    // Cap at 8 threads (diminishing returns for TTS beyond this)
    if (cpu_count <= 2) {
        return 1;  // Low-end: single thread to avoid contention
    } else if (cpu_count <= 4) {
        return cpu_count - 1;  // Mid-range: 2-3 threads
    } else if (cpu_count <= 8) {
        return cpu_count - 2;  // High-end: leave 2 cores free
    } else {
        return 8;  // Cap at 8 for TTS
    }
}

void TextToSpeech::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("load_model", "model_path", "voices_path", "tokens_path", "data_dir", "lexicon", "dict_dir", "lang"),
                         &TextToSpeech::load_model, DEFVAL(""), DEFVAL(""), DEFVAL(""), DEFVAL(""));
    ClassDB::bind_method(D_METHOD("is_model_loaded"), &TextToSpeech::is_model_loaded);
    ClassDB::bind_method(D_METHOD("speak", "text"), &TextToSpeech::speak);
    ClassDB::bind_method(D_METHOD("speak_async", "text"), &TextToSpeech::speak_async);
    ClassDB::bind_method(D_METHOD("speak_streaming", "text"), &TextToSpeech::speak_streaming);
    ClassDB::bind_static_method("TextToSpeech", D_METHOD("split_into_chunks", "text"), &TextToSpeech::split_into_chunks);
    ClassDB::bind_method(D_METHOD("is_generating"), &TextToSpeech::is_generating);
    ClassDB::bind_method(D_METHOD("cancel_generation"), &TextToSpeech::cancel_generation);
    ClassDB::bind_method(D_METHOD("get_speaker_count"), &TextToSpeech::get_speaker_count);
    ClassDB::bind_method(D_METHOD("get_sample_rate"), &TextToSpeech::get_sample_rate);
    ClassDB::bind_static_method("TextToSpeech", D_METHOD("get_optimal_thread_count"), &TextToSpeech::get_optimal_thread_count);

    // Property getters/setters - voice
    ClassDB::bind_method(D_METHOD("set_speaker_id", "id"), &TextToSpeech::set_speaker_id);
    ClassDB::bind_method(D_METHOD("get_speaker_id"), &TextToSpeech::get_speaker_id);
    ClassDB::bind_method(D_METHOD("set_speed", "speed"), &TextToSpeech::set_speed);
    ClassDB::bind_method(D_METHOD("get_speed"), &TextToSpeech::get_speed);

    // Property getters/setters - performance
    ClassDB::bind_method(D_METHOD("set_num_threads", "threads"), &TextToSpeech::set_num_threads);
    ClassDB::bind_method(D_METHOD("get_num_threads"), &TextToSpeech::get_num_threads);
    ClassDB::bind_method(D_METHOD("set_debug_mode", "enabled"), &TextToSpeech::set_debug_mode);
    ClassDB::bind_method(D_METHOD("get_debug_mode"), &TextToSpeech::get_debug_mode);
    ClassDB::bind_method(D_METHOD("set_max_sentences", "count"), &TextToSpeech::set_max_sentences);
    ClassDB::bind_method(D_METHOD("get_max_sentences"), &TextToSpeech::get_max_sentences);

    // Properties - voice
    ADD_PROPERTY(PropertyInfo(Variant::INT, "speaker_id", PROPERTY_HINT_RANGE, "0,100,1"),
                 "set_speaker_id", "get_speaker_id");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0.1,3.0,0.1"),
                 "set_speed", "get_speed");

    // Properties - performance
    ADD_PROPERTY(PropertyInfo(Variant::INT, "num_threads", PROPERTY_HINT_RANGE, "0,16,1"),
                 "set_num_threads", "get_num_threads");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_mode"),
                 "set_debug_mode", "get_debug_mode");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_sentences", PROPERTY_HINT_RANGE, "1,10,1"),
                 "set_max_sentences", "get_max_sentences");

    // Signals
    ADD_SIGNAL(MethodInfo("model_loaded"));
    ADD_SIGNAL(MethodInfo("speech_generated", PropertyInfo(Variant::OBJECT, "audio")));
    ADD_SIGNAL(MethodInfo("generation_started", PropertyInfo(Variant::INT, "request_id")));
    ADD_SIGNAL(MethodInfo("generation_completed", PropertyInfo(Variant::INT, "request_id"), PropertyInfo(Variant::OBJECT, "audio")));
    ADD_SIGNAL(MethodInfo("generation_failed", PropertyInfo(Variant::INT, "request_id"), PropertyInfo(Variant::STRING, "error")));

    // Streaming signals
    ADD_SIGNAL(MethodInfo("chunk_ready",
        PropertyInfo(Variant::INT, "request_id"),
        PropertyInfo(Variant::INT, "chunk_index"),
        PropertyInfo(Variant::INT, "total_chunks"),
        PropertyInfo(Variant::OBJECT, "audio")));
    ADD_SIGNAL(MethodInfo("stream_completed", PropertyInfo(Variant::INT, "request_id")));
}

TextToSpeech::TextToSpeech() {
    tts = nullptr;
    speaker_id = 0;
    speed = 1.0f;
    model_loaded = false;
}

TextToSpeech::~TextToSpeech() {
    // Stop worker thread first
    stop_worker_thread();

    if (tts) {
        SherpaOnnxDestroyOfflineTts(tts);
        tts = nullptr;
    }
}

void TextToSpeech::start_worker_thread() {
    if (thread_running.load()) return;

    should_exit.store(false);
    worker_thread = std::thread(&TextToSpeech::worker_thread_func, this);
    thread_running.store(true);
}

void TextToSpeech::stop_worker_thread() {
    if (!thread_running.load()) return;

    should_exit.store(true);

    // Wake up the worker thread so it can exit
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        work_condition.notify_one();
    }

    if (worker_thread.joinable()) {
        worker_thread.join();
    }

    thread_running.store(false);
}

// Helper to check if path is already absolute (Windows drive letter or Unix root)
static bool is_absolute_path(const String &path) {
    if (path.length() < 2) return false;
    // Windows: C:/ or C:\
    if (path[1] == ':') return true;
    // Unix: /
    if (path[0] == '/') return true;
    return false;
}

// Helper to convert path - only globalize if it's a res:// path
static String resolve_path(const String &path) {
    if (path.is_empty()) return path;
    if (is_absolute_path(path)) {
        // Already absolute, just return it
        return path;
    }
    // Use Godot's globalize_path for res:// paths
    return ProjectSettings::get_singleton()->globalize_path(path);
}

void TextToSpeech::load_model(const String &model, const String &voices, const String &tokens, const String &data_dir,
                              const String &lexicon, const String &dict, const String &language) {
    // Cleanup existing model
    if (tts) {
        SherpaOnnxDestroyOfflineTts(tts);
        tts = nullptr;
        model_loaded = false;
    }

    // Convert paths to absolute paths (handles both res:// and already-absolute paths)
    String abs_model = resolve_path(model);
    String abs_voices = resolve_path(voices);
    String abs_tokens = resolve_path(tokens);
    String abs_data_dir = resolve_path(data_dir);
    String abs_lexicon = resolve_path(lexicon);
    String abs_dict = resolve_path(dict);

    // Store paths
    model_path = abs_model;
    voices_path = abs_voices;
    tokens_path = abs_tokens;
    lexicon_path = abs_lexicon;
    dict_dir = abs_dict;
    lang = language;

    // Convert to UTF8 - must keep these alive until after API call
    CharString model_utf8 = abs_model.utf8();
    CharString voices_utf8 = abs_voices.utf8();
    CharString tokens_utf8 = abs_tokens.utf8();
    CharString data_dir_utf8 = abs_data_dir.utf8();
    CharString lexicon_utf8 = abs_lexicon.utf8();
    CharString dict_utf8 = abs_dict.utf8();
    CharString lang_utf8 = language.utf8();

    UtilityFunctions::print("TextToSpeech: Loading model from:");
    UtilityFunctions::print("  Model: ", abs_model);
    UtilityFunctions::print("  Voices: ", abs_voices);
    UtilityFunctions::print("  Tokens: ", abs_tokens);
    UtilityFunctions::print("  Data dir: ", abs_data_dir);
    if (!abs_lexicon.is_empty()) {
        UtilityFunctions::print("  Lexicon: ", abs_lexicon);
    }
    if (!abs_dict.is_empty()) {
        UtilityFunctions::print("  Dict dir: ", abs_dict);
    }
    if (!language.is_empty()) {
        UtilityFunctions::print("  Language: ", language);
    }

    // Initialize config
    SherpaOnnxOfflineTtsConfig config;
    memset(&config, 0, sizeof(config));

    // Set Kokoro model config
    config.model.kokoro.model = model_utf8.get_data();
    config.model.kokoro.voices = voices_utf8.get_data();
    config.model.kokoro.tokens = tokens_utf8.get_data();
    config.model.kokoro.data_dir = data_dir_utf8.get_data();
    config.model.kokoro.length_scale = 1.0f;
    config.model.kokoro.dict_dir = dict_utf8.get_data();
    config.model.kokoro.lexicon = lexicon_utf8.get_data();
    config.model.kokoro.lang = lang_utf8.get_data();

    // General model config - use dynamic thread count
    int effective_threads = (num_threads <= 0) ? get_optimal_thread_count() : num_threads;
    config.model.num_threads = effective_threads;
    config.model.debug = debug_mode ? 1 : 0;
    config.model.provider = "cpu";

    // TTS config
    config.max_num_sentences = max_sentences;

    UtilityFunctions::print("TextToSpeech: Using ", effective_threads, " CPU threads (debug=", debug_mode ? "on" : "off", ")");

    // Create TTS engine
    tts = SherpaOnnxCreateOfflineTts(&config);

    if (tts) {
        model_loaded = true;
        UtilityFunctions::print("TextToSpeech: Model loaded successfully");
        UtilityFunctions::print("  Speakers: ", get_speaker_count());
        UtilityFunctions::print("  Sample rate: ", get_sample_rate(), " Hz");
        emit_signal("model_loaded");
    } else {
        UtilityFunctions::printerr("TextToSpeech: Failed to load model");
        UtilityFunctions::printerr("  Model: ", model);
        UtilityFunctions::printerr("  Voices: ", voices);
        UtilityFunctions::printerr("  Tokens: ", tokens);
    }
}

bool TextToSpeech::is_model_loaded() const {
    return model_loaded && tts != nullptr;
}

// Internal audio generation with buffer reuse (thread-safe for worker thread)
Ref<AudioStreamWAV> TextToSpeech::generate_audio_internal(const String &text, int sid, float spd) {
    Ref<AudioStreamWAV> wav;

    if (!tts) {
        return wav;
    }

    // Keep text UTF8 alive during API call
    CharString text_utf8 = text.utf8();

    // Generate audio
    const SherpaOnnxGeneratedAudio *audio =
        SherpaOnnxOfflineTtsGenerate(tts, text_utf8.get_data(), sid, spd);

    if (!audio || audio->n <= 0) {
        if (audio) {
            SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
        }
        return wav;
    }

    // Reuse PCM buffer if large enough, otherwise resize
    size_t required_size = static_cast<size_t>(audio->n) * 2;
    if (pcm_buffer.size() < static_cast<int64_t>(required_size)) {
        pcm_buffer.resize(required_size);
    }

    int16_t *pcm = reinterpret_cast<int16_t*>(pcm_buffer.ptrw());

    // Convert float samples to 16-bit PCM (SIMD-friendly loop)
    for (int32_t i = 0; i < audio->n; i++) {
        float sample = audio->samples[i];
        // Clamp to [-1, 1] and convert to 16-bit
        sample = (sample > 1.0f) ? 1.0f : ((sample < -1.0f) ? -1.0f : sample);
        pcm[i] = static_cast<int16_t>(sample * 32767.0f);
    }

    int sample_rate = audio->sample_rate;
    int num_samples = audio->n;

    // Cleanup sherpa audio
    SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);

    // Create Godot AudioStreamWAV with a copy of the data
    PackedByteArray audio_data;
    audio_data.resize(num_samples * 2);
    memcpy(audio_data.ptrw(), pcm_buffer.ptr(), num_samples * 2);

    wav.instantiate();
    wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
    wav->set_mix_rate(sample_rate);
    wav->set_stereo(false);
    wav->set_data(audio_data);

    return wav;
}

// Synchronous speech generation (blocks until complete)
Ref<AudioStreamWAV> TextToSpeech::speak(const String &text) {
    if (!tts) {
        UtilityFunctions::printerr("TextToSpeech: Model not loaded");
        return Ref<AudioStreamWAV>();
    }

    if (text.is_empty()) {
        UtilityFunctions::printerr("TextToSpeech: Empty text");
        return Ref<AudioStreamWAV>();
    }

    if (debug_mode) {
        UtilityFunctions::print("TextToSpeech: Generating speech for: ", text);
        UtilityFunctions::print("  Speaker ID: ", speaker_id, ", Speed: ", speed);
    }

    Ref<AudioStreamWAV> wav = generate_audio_internal(text, speaker_id, speed);

    if (wav.is_valid()) {
        if (debug_mode) {
            UtilityFunctions::print("TextToSpeech: Generated audio, duration: ",
                (float)wav->get_data().size() / 2 / wav->get_mix_rate(), " seconds");
        }
        emit_signal("speech_generated", wav);
    } else {
        UtilityFunctions::printerr("TextToSpeech: Failed to generate audio");
    }

    return wav;
}

// Async speech generation (non-blocking)
uint64_t TextToSpeech::speak_async(const String &text) {
    if (!tts) {
        UtilityFunctions::printerr("TextToSpeech: Model not loaded");
        return 0;
    }

    if (text.is_empty()) {
        UtilityFunctions::printerr("TextToSpeech: Empty text");
        return 0;
    }

    // Start worker thread if not running
    if (!thread_running.load()) {
        start_worker_thread();
    }

    uint64_t request_id = next_request_id.fetch_add(1);

    TTSRequest request;
    request.text = text;
    request.speaker_id = speaker_id;
    request.speed = speed;
    request.request_id = request_id;

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        request_queue.push(request);
        work_condition.notify_one();
    }

    // Emit signal using call_deferred for thread safety
    call_deferred("emit_signal", "generation_started", request_id);

    if (debug_mode) {
        UtilityFunctions::print("TextToSpeech: Queued async request #", request_id, " for: ", text);
    }

    return request_id;
}

void TextToSpeech::worker_thread_func() {
    while (!should_exit.load()) {
        bool has_work = false;
        bool is_chunk = false;
        TTSRequest request;
        TTSChunk chunk;

        // Wait for work (either regular request or chunk)
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            work_condition.wait(lock, [this] {
                return should_exit.load() || !request_queue.empty() || !chunk_queue.empty();
            });

            if (should_exit.load()) break;

            // Prioritize chunks for lower latency streaming
            if (!chunk_queue.empty()) {
                chunk = chunk_queue.front();
                chunk_queue.pop();
                is_chunk = true;
                has_work = true;
            } else if (!request_queue.empty()) {
                request = request_queue.front();
                request_queue.pop();
                is_chunk = false;
                has_work = true;
            }
        }

        if (!has_work) continue;

        generation_in_progress.store(true);

        if (is_chunk) {
            // Generate audio for chunk
            TTSChunkResult result;
            result.request_id = chunk.request_id;
            result.chunk_index = chunk.chunk_index;
            result.total_chunks = chunk.total_chunks;
            result.audio = generate_audio_internal(chunk.text, chunk.speaker_id, chunk.speed);
            result.success = result.audio.is_valid();

            if (!result.success) {
                result.error_message = "Failed to generate chunk audio";
            }

            // Store chunk result for main thread
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                chunk_result_queue.push(result);
            }
        } else {
            // Generate audio for regular request
            TTSResult result;
            result.request_id = request.request_id;
            result.audio = generate_audio_internal(request.text, request.speaker_id, request.speed);
            result.success = result.audio.is_valid();

            if (!result.success) {
                result.error_message = "Failed to generate audio";
            }

            // Store result for main thread
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                result_queue.push(result);
            }
        }

        generation_in_progress.store(false);
    }
}

void TextToSpeech::process_pending_results() {
    std::lock_guard<std::mutex> lock(result_mutex);

    // Process regular results
    while (!result_queue.empty()) {
        TTSResult result = result_queue.front();
        result_queue.pop();

        if (result.success) {
            emit_signal("generation_completed", result.request_id, result.audio);
            emit_signal("speech_generated", result.audio);  // Backwards compatibility
        } else {
            emit_signal("generation_failed", result.request_id, result.error_message);
        }
    }

    // Process chunk results for streaming
    while (!chunk_result_queue.empty()) {
        TTSChunkResult result = chunk_result_queue.front();
        chunk_result_queue.pop();

        if (result.success) {
            emit_signal("chunk_ready", result.request_id, result.chunk_index,
                        result.total_chunks, result.audio);

            // If this was the last chunk, emit stream_completed
            if (result.chunk_index == result.total_chunks - 1) {
                emit_signal("stream_completed", result.request_id);
            }
        } else {
            emit_signal("generation_failed", result.request_id, result.error_message);
        }
    }
}

void TextToSpeech::_process(double delta) {
    // Check for completed async results and emit signals on main thread
    process_pending_results();
}

bool TextToSpeech::is_generating() const {
    return generation_in_progress.load() || !request_queue.empty() || !chunk_queue.empty();
}

void TextToSpeech::cancel_generation() {
    // Clear pending requests
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!request_queue.empty()) {
            request_queue.pop();
        }
        while (!chunk_queue.empty()) {
            chunk_queue.pop();
        }
    }
    // Note: Cannot cancel in-progress generation without modifying sherpa-onnx
}

// Split text into chunks for streaming TTS
PackedStringArray TextToSpeech::split_into_chunks(const String &text) {
    PackedStringArray chunks;

    if (text.is_empty()) {
        return chunks;
    }

    // Split by sentence-ending punctuation: . ! ?
    String current_chunk;
    bool in_sentence = false;

    for (int i = 0; i < text.length(); i++) {
        char32_t c = text[i];
        current_chunk += String::chr(c);

        // Check for sentence-ending punctuation
        if (c == '.' || c == '!' || c == '?') {
            // Look ahead for closing quotes or parentheses
            while (i + 1 < text.length()) {
                char32_t next = text[i + 1];
                if (next == '"' || next == '\'' || next == ')' || next == ']') {
                    i++;
                    current_chunk += String::chr(next);
                } else {
                    break;
                }
            }

            // Trim and add chunk if not empty
            String trimmed = current_chunk.strip_edges();
            if (!trimmed.is_empty()) {
                chunks.push_back(trimmed);
            }
            current_chunk = "";
            in_sentence = false;
        } else if (c != ' ' && c != '\t' && c != '\n') {
            in_sentence = true;
        }
    }

    // Add remaining text as final chunk
    String trimmed = current_chunk.strip_edges();
    if (!trimmed.is_empty()) {
        chunks.push_back(trimmed);
    }

    // If no chunks created (text without punctuation), split by word count
    if (chunks.is_empty() && !text.strip_edges().is_empty()) {
        chunks.push_back(text.strip_edges());
    }

    return chunks;
}

// Streaming speech generation (low-latency chunked)
uint64_t TextToSpeech::speak_streaming(const String &text) {
    if (!tts) {
        UtilityFunctions::printerr("TextToSpeech: Model not loaded");
        return 0;
    }

    if (text.is_empty()) {
        UtilityFunctions::printerr("TextToSpeech: Empty text");
        return 0;
    }

    // Split text into chunks
    PackedStringArray chunks = split_into_chunks(text);

    if (chunks.is_empty()) {
        UtilityFunctions::printerr("TextToSpeech: No chunks created from text");
        return 0;
    }

    // Start worker thread if not running
    if (!thread_running.load()) {
        start_worker_thread();
    }

    uint64_t request_id = next_request_id.fetch_add(1);
    int total_chunks = chunks.size();

    // Queue all chunks for generation
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        for (int i = 0; i < total_chunks; i++) {
            TTSChunk chunk;
            chunk.text = chunks[i];
            chunk.speaker_id = speaker_id;
            chunk.speed = speed;
            chunk.request_id = request_id;
            chunk.chunk_index = i;
            chunk.total_chunks = total_chunks;
            chunk.is_streaming = true;
            chunk_queue.push(chunk);
        }
        work_condition.notify_one();
    }

    // Emit signal using call_deferred for thread safety
    call_deferred("emit_signal", "generation_started", request_id);

    if (debug_mode) {
        UtilityFunctions::print("TextToSpeech: Queued streaming request #", request_id,
            " with ", total_chunks, " chunks");
        for (int i = 0; i < total_chunks; i++) {
            UtilityFunctions::print("  Chunk ", i, ": ", chunks[i]);
        }
    }

    return request_id;
}

void TextToSpeech::set_speaker_id(int id) {
    speaker_id = id;
}

int TextToSpeech::get_speaker_id() const {
    return speaker_id;
}

void TextToSpeech::set_speed(float s) {
    speed = s;
}

float TextToSpeech::get_speed() const {
    return speed;
}

int TextToSpeech::get_speaker_count() const {
    if (!tts) return 0;
    return SherpaOnnxOfflineTtsNumSpeakers(tts);
}

int TextToSpeech::get_sample_rate() const {
    if (!tts) return 0;
    return SherpaOnnxOfflineTtsSampleRate(tts);
}

// Performance property setters/getters
void TextToSpeech::set_num_threads(int threads) {
    num_threads = threads;
}

int TextToSpeech::get_num_threads() const {
    return num_threads;
}

void TextToSpeech::set_debug_mode(bool enabled) {
    debug_mode = enabled;
}

bool TextToSpeech::get_debug_mode() const {
    return debug_mode;
}

void TextToSpeech::set_max_sentences(int count) {
    max_sentences = count;
}

int TextToSpeech::get_max_sentences() const {
    return max_sentences;
}
