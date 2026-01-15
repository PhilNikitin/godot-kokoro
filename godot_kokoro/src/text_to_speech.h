#ifndef TEXT_TO_SPEECH_H
#define TEXT_TO_SPEECH_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

// Forward declaration - sherpa-onnx C API types
typedef struct SherpaOnnxOfflineTts SherpaOnnxOfflineTts;

namespace godot {

// Request structure for async TTS
struct TTSRequest {
    String text;
    int speaker_id;
    float speed;
    uint64_t request_id;
};

// Result structure for async TTS
struct TTSResult {
    Ref<AudioStreamWAV> audio;
    uint64_t request_id;
    bool success;
    String error_message;
};

// Chunk structure for streaming TTS
struct TTSChunk {
    String text;
    int speaker_id;
    float speed;
    uint64_t request_id;
    int chunk_index;
    int total_chunks;
    bool is_streaming;  // true = streaming mode, false = regular async
};

// Chunk result for streaming TTS
struct TTSChunkResult {
    Ref<AudioStreamWAV> audio;
    uint64_t request_id;
    int chunk_index;
    int total_chunks;
    bool success;
    String error_message;
};

class TextToSpeech : public Node {
    GDCLASS(TextToSpeech, Node)

private:
    const SherpaOnnxOfflineTts *tts = nullptr;
    String model_path;
    String voices_path;
    String tokens_path;
    String lexicon_path;  // For multi-lang models
    String dict_dir;      // For multi-lang models
    String lang;          // Language code (e.g., "en-us", "zh", "ja")
    int speaker_id = 0;
    float speed = 1.0f;
    bool model_loaded = false;

    // Performance configuration
    int num_threads = 0;        // 0 = auto-detect
    bool debug_mode = false;    // Debug output disabled by default
    int max_sentences = 2;      // Sentence batching

    // Reusable buffer for PCM conversion
    PackedByteArray pcm_buffer;

    // Threading infrastructure for async generation
    std::thread worker_thread;
    std::atomic<bool> thread_running{false};
    std::atomic<bool> should_exit{false};
    std::mutex queue_mutex;
    std::mutex result_mutex;
    std::condition_variable work_condition;
    std::queue<TTSRequest> request_queue;
    std::queue<TTSResult> result_queue;
    std::atomic<uint64_t> next_request_id{1};
    std::atomic<bool> generation_in_progress{false};

    // Streaming infrastructure
    std::queue<TTSChunk> chunk_queue;
    std::queue<TTSChunkResult> chunk_result_queue;

    // Internal methods
    void worker_thread_func();
    void process_pending_results();
    Ref<AudioStreamWAV> generate_audio_internal(const String &text, int sid, float spd);
    void start_worker_thread();
    void stop_worker_thread();

protected:
    static void _bind_methods();

public:
    TextToSpeech();
    ~TextToSpeech();

    // Model loading
    void load_model(const String &model, const String &voices, const String &tokens, const String &data_dir = "",
                    const String &lexicon = "", const String &dict = "", const String &language = "");
    bool is_model_loaded() const;

    // Synchronous speech generation (blocks until complete)
    Ref<AudioStreamWAV> speak(const String &text);

    // Async speech generation (non-blocking)
    uint64_t speak_async(const String &text);
    bool is_generating() const;
    void cancel_generation();

    // Streaming speech generation (low-latency chunked)
    uint64_t speak_streaming(const String &text);
    static PackedStringArray split_into_chunks(const String &text);

    // Called each frame to check for completed async generations
    void _process(double delta);

    // Properties - voice
    void set_speaker_id(int id);
    int get_speaker_id() const;
    void set_speed(float s);
    float get_speed() const;

    // Properties - performance
    void set_num_threads(int threads);
    int get_num_threads() const;
    void set_debug_mode(bool enabled);
    bool get_debug_mode() const;
    void set_max_sentences(int count);
    int get_max_sentences() const;

    // Utility
    int get_speaker_count() const;
    int get_sample_rate() const;
    static int get_optimal_thread_count();
};

} // namespace godot

#endif // TEXT_TO_SPEECH_H
