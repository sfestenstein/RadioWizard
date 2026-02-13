#ifndef COMMONUTILS_CIRCULARBUFFER_H_
#define COMMONUTILS_CIRCULARBUFFER_H_

// System headers
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace CommonUtils
{

/// Thread-unsafe, fixed-capacity circular buffer for time-series history.
/// Stores the most recent `capacity` elements; older entries are silently
/// overwritten.
template <typename T>
class CircularBuffer
{
public:
   /// Construct a buffer with the given maximum capacity.
   /// @throws std::invalid_argument if capacity is zero.
   explicit CircularBuffer(std::size_t capacity)
      : _data(capacity)
      , _capacity{capacity}
   {
      if (capacity == 0)
      {
         throw std::invalid_argument("CircularBuffer capacity must be > 0");
      }
   }

   /// Push a single element, overwriting the oldest if full.
   void push(const T& value)
   {
      _data[_head] = value;
      _head = (_head + 1) % _capacity;
      if (_size < _capacity)
      {
         ++_size;
      }
   }

   /// Push a range of elements.
   void push(const T* data, std::size_t count)
   {
      for (std::size_t i = 0; i < count; ++i)
      {
         push(data[i]);
      }
   }

   /// Push a vector of elements.
   void push(const std::vector<T>& values)
   {
      push(values.data(), values.size());
   }

   /// Access element by logical index (0 = oldest).
   [[nodiscard]] const T& operator[](std::size_t index) const
   {
      return _data[physicalIndex(index)];
   }

   /// Access element by logical index with bounds checking.
   [[nodiscard]] const T& at(std::size_t index) const
   {
      if (index >= _size)
      {
         throw std::out_of_range("CircularBuffer::at index out of range");
      }
      return _data[physicalIndex(index)];
   }

   /// Return the most recently pushed element.
   [[nodiscard]] const T& back() const
   {
      if (_size == 0)
      {
         throw std::out_of_range("CircularBuffer::back called on empty buffer");
      }
      return _data[(_head + _capacity - 1) % _capacity];
   }

   /// Copy all stored elements in chronological order into a vector.
   [[nodiscard]] std::vector<T> toVector() const
   {
      std::vector<T> result;
      result.reserve(_size);
      for (std::size_t i = 0; i < _size; ++i)
      {
         result.push_back((*this)[i]);
      }
      return result;
   }

   /// Number of elements currently stored.
   [[nodiscard]] std::size_t size() const { return _size; }

   /// Maximum number of elements the buffer can hold.
   [[nodiscard]] std::size_t capacity() const { return _capacity; }

   /// True if the buffer has been filled at least once.
   [[nodiscard]] bool full() const { return _size == _capacity; }

   /// True if no elements have been stored.
   [[nodiscard]] bool empty() const { return _size == 0; }

   /// Remove all elements without changing capacity.
   void clear()
   {
      _head = 0;
      _size = 0;
   }

private:
   [[nodiscard]] std::size_t physicalIndex(std::size_t logicalIndex) const
   {
      if (_size < _capacity)
      {
         return logicalIndex;
      }
      return (_head + logicalIndex) % _capacity;
   }

   std::vector<T> _data;
   std::size_t _capacity;
   std::size_t _head{0};
   std::size_t _size{0};
};

} // namespace CommonUtils

#endif // COMMONUTILS_CIRCULARBUFFER_H_
