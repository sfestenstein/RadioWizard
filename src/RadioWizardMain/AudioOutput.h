#ifndef AUDIOOUTPUT_H_
#define AUDIOOUTPUT_H_

// Third-party headers (Qt)
#include <QAudioSink>
#include <QIODevice>

// System headers
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

/**
 * @class AudioBuffer
 * @brief QIODevice subclass wrapping a thread-safe ring buffer for audio data.
 *
 * Used internally by AudioOutput in pull mode — QAudioSink reads PCM data
 * from this device, while samples are fed from any thread via feedSamples().
 */
class AudioBuffer : public QIODevice
{
   Q_OBJECT

public:
   explicit AudioBuffer(size_t capacity, QObject* parent = nullptr);

   /// Feed interleaved float samples from any thread.
   void feedSamples(const std::vector<float>& samples);

   /// Reset the ring buffer to empty.
   void clear();

protected:
   qint64 readData(char* data, qint64 maxlen) override;
   qint64 writeData(const char* data, qint64 len) override;
   [[nodiscard]] qint64 bytesAvailable() const override;

private:
   mutable std::mutex _mutex;
   std::vector<float> _ring;
   size_t _readPos{0};
   size_t _writePos{0};
   size_t _count{0};
   size_t _capacity;
};

/**
 * @class AudioOutput
 * @brief Plays stereo (or mono) float audio samples through the default audio
 *        output device using Qt 6 QAudioSink (cross-platform).
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
    * Samples are buffered internally and consumed by the audio sink.
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
   static constexpr size_t RING_CAPACITY = 512UL * 1024UL; // ~5 s stereo @ 48 kHz

   double _sampleRate;
   int _numChannels;
   std::atomic<float> _volume{0.8F};
   std::atomic<bool> _playing{false};

   std::unique_ptr<QAudioSink> _audioSink;
   std::unique_ptr<AudioBuffer> _buffer;
};

#endif // AUDIOOUTPUT_H_
