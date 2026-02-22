// Project headers
#include "AudioOutput.h"
#include "GeneralLogger.h"

// System headers
#include <algorithm>
#include <cstring>

// ============================================================================
// Construction / destruction
// ============================================================================

AudioOutput::AudioOutput(double sampleRate, int numChannels)
   : _sampleRate(sampleRate)
   , _numChannels(numChannels)
   , _ringBuffer(RING_CAPACITY, 0.0F)
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

   // Describe the audio format: N-channel, 32-bit float, native endian.
   AudioStreamBasicDescription format{};
   format.mSampleRate = _sampleRate;
   format.mFormatID = kAudioFormatLinearPCM;
   format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
   format.mBitsPerChannel = 32;
   format.mChannelsPerFrame = static_cast<UInt32>(_numChannels);
   format.mFramesPerPacket = 1;
   format.mBytesPerFrame = static_cast<UInt32>(sizeof(float) * _numChannels);
   format.mBytesPerPacket = static_cast<UInt32>(sizeof(float) * _numChannels);

   // Create the output audio queue.
   OSStatus status = AudioQueueNewOutput(
      &format,
      audioQueueCallback,
      this,      // userData
      nullptr,   // run loop (nullptr = internal)
      nullptr,   // run loop mode
      0,         // flags
      &_queue);

   if (status != noErr)
   {
      GPERROR("AudioQueueNewOutput failed: {}", static_cast<int>(status));
      return false;
   }

   // Allocate and prime buffers.
   // BUFFER_SAMPLES is the number of *frames*; total floats = frames × channels.
   const auto bufferSize =
      static_cast<UInt32>(BUFFER_SAMPLES * _numChannels * sizeof(float));
   for (int i = 0; i < NUM_BUFFERS; ++i)
   {
      status = AudioQueueAllocateBuffer(_queue, bufferSize, &_buffers[i]);
      if (status != noErr)
      {
         GPERROR("AudioQueueAllocateBuffer failed: {}",
                 static_cast<int>(status));
         AudioQueueDispose(_queue, true);
         _queue = nullptr;
         return false;
      }
      // Prime with silence so the queue starts immediately.
      fillBuffer(_buffers[i]);
      AudioQueueEnqueueBuffer(_queue, _buffers[i], 0, nullptr);
   }

   // Set volume.
   AudioQueueSetParameter(_queue, kAudioQueueParam_Volume,
                          static_cast<Float32>(_volume.load()));

   // Start playback.
   status = AudioQueueStart(_queue, nullptr);
   if (status != noErr)
   {
      GPERROR("AudioQueueStart failed: {}", static_cast<int>(status));
      AudioQueueDispose(_queue, true);
      _queue = nullptr;
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

   if (_queue != nullptr)
   {
      AudioQueueStop(_queue, true); // immediate stop
      AudioQueueDispose(_queue, true);
      _queue = nullptr;
   }

   // Clear the ring buffer.
   {
      std::lock_guard lock(_ringMutex);
      _ringReadPos = 0;
      _ringWritePos = 0;
      _ringCount = 0;
   }

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

   std::lock_guard lock(_ringMutex);

   for (float sample : samples)
   {
      if (_ringCount < RING_CAPACITY)
      {
         _ringBuffer[_ringWritePos] = sample;
         _ringWritePos = (_ringWritePos + 1) % RING_CAPACITY;
         ++_ringCount;
      }
      // Drop samples if ring is full (prevents unbounded growth).
   }
}

void AudioOutput::setVolume(float volume)
{
   _volume.store(std::clamp(volume, 0.0F, 1.0F));
   if (_queue != nullptr)
   {
      AudioQueueSetParameter(_queue, kAudioQueueParam_Volume,
                             static_cast<Float32>(_volume.load()));
   }
}

// ============================================================================
// AudioQueue callback
// ============================================================================

void AudioOutput::audioQueueCallback(void* userData,
                                     AudioQueueRef queue,
                                     AudioQueueBufferRef buffer)
{
   auto* self = static_cast<AudioOutput*>(userData);
   if (self == nullptr || !self->_playing.load())
   {
      // Fill with silence and re-enqueue to keep the queue alive.
      const auto totalFloats =
         static_cast<size_t>(BUFFER_SAMPLES * (self ? self->_numChannels : 2));
      std::memset(buffer->mAudioData, 0, totalFloats * sizeof(float));
      buffer->mAudioDataByteSize =
         static_cast<UInt32>(totalFloats * sizeof(float));
      AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
      return;
   }

   self->fillBuffer(buffer);
   AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
}

void AudioOutput::fillBuffer(AudioQueueBufferRef buffer)
{
   auto* dest = static_cast<float*>(buffer->mAudioData);
   // Total float values needed = frames × channels.
   const auto requestedFloats =
      static_cast<size_t>(BUFFER_SAMPLES * _numChannels);

   std::lock_guard lock(_ringMutex);

   size_t available = _ringCount;
   size_t toCopy = std::min(available, requestedFloats);

   for (size_t i = 0; i < toCopy; ++i)
   {
      dest[i] = _ringBuffer[_ringReadPos];
      _ringReadPos = (_ringReadPos + 1) % RING_CAPACITY;
   }
   _ringCount -= toCopy;

   // Zero-fill remainder (silence).
   if (toCopy < requestedFloats)
   {
      std::memset(dest + toCopy, 0,
                  (requestedFloats - toCopy) * sizeof(float));
   }

   buffer->mAudioDataByteSize =
      static_cast<UInt32>(requestedFloats * sizeof(float));
}
