#ifndef AUDIOOUTPUT_H_
#define AUDIOOUTPUT_H_

// System headers
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// macOS CoreAudio
#include <AudioToolbox/AudioQueue.h>

/**
 * @class AudioOutput
 * @brief Plays stereo float audio samples through the default audio output
 *        device using macOS AudioQueue Services.
 *
 * Usage:
 *   1. Construct with desired sample rate.
 *   2. Call start() to begin playback.
 *   3. Feed samples via pushSamples() from any thread.
 *   4. Call stop() to halt.
 *
 * Thread-safety: pushSamples() is thread-safe.
 */
class AudioOutput
{
public:
   /**
    * @brief Construct an AudioOutput for the given sample rate.
    * @param sampleRate  Audio sample rate in Hz (e.g. 48000).
    * @param numChannels Number of audio channels (1 = mono, 2 = stereo).
    */
   explicit AudioOutput(double sampleRate = 48000.0, int numChannels = 2);

   /**
    * @brief Destroy the AudioOutput and release resources.
    */
   ~AudioOutput();

   // Non-copyable, non-movable.
   AudioOutput(const AudioOutput&) = delete;
   AudioOutput& operator=(const AudioOutput&) = delete;
   AudioOutput(AudioOutput&&) = delete;
   AudioOutput& operator=(AudioOutput&&) = delete;

   /**
    * @brief Start audio playback.
    * @return true on success.
    */
   [[nodiscard]] bool start();

   /**
    * @brief Stop audio playback.
    */
   void stop();

   /**
    * @brief Check if audio is playing.
    * @return true if audio is playing.
    */
   [[nodiscard]] bool isPlaying() const;

   /**
    * @brief Push interleaved stereo (or mono) audio samples for playback.
    *
    * Samples are buffered internally and consumed by the audio queue.
    * Safe to call from any thread.
    *
    * For stereo: samples are interleaved [L, R, L, R, ...].
    * For mono: single stream of samples.
    *
    * @param samples  Float audio samples (normalised to [-1, 1]).
    */
   void pushSamples(const std::vector<float>& samples);

   /**
    * @brief Set the playback volume.
    * @param volume  Volume level [0.0, 1.0].
    */
   void setVolume(float volume);

private:
   static constexpr int NUM_BUFFERS = 3;
   static constexpr int BUFFER_SAMPLES = 4096;

   // AudioQueue callback — fills a buffer from the internal ring.
   static void audioQueueCallback(void* userData,
                                  AudioQueueRef queue,
                                  AudioQueueBufferRef buffer);

   void fillBuffer(AudioQueueBufferRef buffer);

   double _sampleRate;
   int _numChannels;
   std::atomic<float> _volume{0.8F};
   std::atomic<bool> _playing{false};

   AudioQueueRef _queue{nullptr};
   AudioQueueBufferRef _buffers[NUM_BUFFERS]{};

   // Lock-protected sample ring buffer.
   std::mutex _ringMutex;
   std::vector<float> _ringBuffer;
   size_t _ringReadPos{0};
   size_t _ringWritePos{0};
   size_t _ringCount{0};
   static constexpr size_t RING_CAPACITY = 512 * 1024; // ~5 seconds stereo at 48 kHz
};

#endif // AUDIOOUTPUT_H_
