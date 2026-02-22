// Project headers
#include "AudioOutput.h"
#include "GeneralLogger.h"

// Third-party headers (Qt)
#include <QAudioFormat>
#include <QMediaDevices>

// System headers
#include <algorithm>
#include <cstring>

// ============================================================================
// AudioBuffer — QIODevice wrapping a thread-safe ring buffer
// ============================================================================

AudioBuffer::AudioBuffer(size_t capacity, QObject* parent)
   : QIODevice(parent)
   , _ring(capacity, 0.0F)
   , _capacity(capacity)
{
}

void AudioBuffer::feedSamples(const std::vector<float>& samples)
{
   const std::lock_guard lock(_mutex);
   for (const float sample : samples)
   {
      if (_count < _capacity)
      {
         _ring[_writePos] = sample;
         _writePos = (_writePos + 1) % _capacity;
         ++_count;
      }
      // Drop samples if ring is full (prevents unbounded growth).
   }
}

void AudioBuffer::clear()
{
   const std::lock_guard lock(_mutex);
   _readPos = 0;
   _writePos = 0;
   _count = 0;
}

qint64 AudioBuffer::readData(char* data, qint64 maxlen)
{
   const std::lock_guard lock(_mutex);

   auto maxFloats = static_cast<size_t>(maxlen) / sizeof(float);
   const size_t toCopy = std::min(maxFloats, _count);
   auto* dest = reinterpret_cast<float*>(data);

   for (size_t i = 0; i < toCopy; ++i)
   {
      dest[i] = _ring[_readPos];
      _readPos = (_readPos + 1) % _capacity;
   }
   _count -= toCopy;

   // Zero-fill remainder so silence plays instead of underrun artifacts.
   const size_t remaining = maxFloats - toCopy;
   if (remaining > 0)
   {
      std::memset(dest + toCopy, 0, remaining * sizeof(float));
   }

   maxFloats *= sizeof(float);
   return static_cast<qint64>(maxFloats);
}

qint64 AudioBuffer::writeData(const char* /*data*/, qint64 /*len*/)
{
   return -1; // Not used — this device is read-only (pull mode).
}

qint64 AudioBuffer::bytesAvailable() const
{
   const std::lock_guard lock(_mutex);
   return static_cast<qint64>(_count * sizeof(float))
        + QIODevice::bytesAvailable();
}

// ============================================================================
// AudioOutput — construction / destruction
// ============================================================================

AudioOutput::AudioOutput(double sampleRate, int numChannels)
   : _sampleRate(sampleRate)
   , _numChannels(numChannels)
   , _buffer(std::make_unique<AudioBuffer>(RING_CAPACITY))
{
}

AudioOutput::~AudioOutput()
{
   stop();
}

// ============================================================================
// Start / stop
// ============================================================================

bool AudioOutput::start()
{
   if (_playing.load())
   {
      return true; // Already playing.
   }

   QAudioFormat format;
   format.setSampleRate(static_cast<int>(_sampleRate));
   format.setChannelCount(_numChannels);
   format.setSampleFormat(QAudioFormat::Float);

   const QAudioDevice device = QMediaDevices::defaultAudioOutput();
   if (!device.isFormatSupported(format))
   {
      GPERROR("AudioOutput: requested format not supported by default device");
      return false;
   }

   _audioSink = std::make_unique<QAudioSink>(device, format);
   _audioSink->setVolume(static_cast<qreal>(_volume.load()));

   // Open the buffer for reading (pull mode — QAudioSink reads from here).
   _buffer->open(QIODevice::ReadOnly);
   _audioSink->start(_buffer.get());

   if (_audioSink->error() != QAudio::NoError)
   {
      GPERROR("AudioOutput: QAudioSink failed to start (error={})",
              static_cast<int>(_audioSink->error()));
      _buffer->close();
      return false;
   }

   _playing.store(true);
   GPINFO("AudioOutput started at {:.0f} Hz, {} channel(s)",
          _sampleRate, _numChannels);
   return true;
}

void AudioOutput::stop()
{
   if (!_playing.load())
   {
      return;
   }

   _playing.store(false);

   if (_audioSink)
   {
      _audioSink->stop();
   }

   if (_buffer->isOpen())
   {
      _buffer->close();
   }

   _buffer->clear();

   GPINFO("AudioOutput stopped");
}

bool AudioOutput::isPlaying() const
{
   return _playing.load();
}

// ============================================================================
// Sample input
// ============================================================================

void AudioOutput::pushSamples(const std::vector<float>& samples)
{
   if (samples.empty() || !_playing.load())
   {
      return;
   }

   _buffer->feedSamples(samples);
}

void AudioOutput::setVolume(float volume)
{
   _volume.store(std::clamp(volume, 0.0F, 1.0F));
   if (_audioSink)
   {
      _audioSink->setVolume(static_cast<qreal>(_volume.load()));
   }
}
