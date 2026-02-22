#ifndef COMMONUTILS_DATAHANDLER_H_
#define COMMONUTILS_DATAHANDLER_H_

// System headers
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>

#include <mutex>
#include <queue>
#include <thread>

namespace CommonUtils
{

/**
 * @class DataHandler
 * @brief Thread-safe queue that dispatches data to registered listeners.
 *
 * Listeners register an std::function to be called when new data is
 * signalled.  All listener callbacks are invoked on a dedicated worker
 * thread, decoupling the producer from the consumers.
 */
template <typename T>
class DataHandler
{
public:
   using Listener = std::function<void(const T&)>;

   /**
    * @brief Construct a DataHandler and start the worker thread.
    */
   DataHandler() : _stopFlag(false)
   {
      _workerThread = std::thread(&DataHandler::processData, this);
   }

   /**
    * @brief Destroy the DataHandler, stopping the worker thread.
    */
   ~DataHandler()
   {
      _stopFlag = true;
      _condVar.notify_all();
      if (_workerThread.joinable())
      {
         _workerThread.join();
      }

      {
         const std::lock_guard<std::mutex> lock(_listenersMutex);
         _listeners.clear();
      }

      while (!_dataQueue.empty())
      {
         _dataQueue.pop();
      }
   }

   // Non-copyable.
   DataHandler(const DataHandler&) = delete;
   DataHandler& operator=(const DataHandler&) = delete;

   /**
    * @brief Push new data to the queue and notify the worker thread.
    *
    * @param data The data item to enqueue.
    */
   void signalData(const T& data)
   {
      if (_stopFlag) return;

      const std::lock_guard<std::mutex> lock(_cvMutex);
      {
         _dataQueue.push(data);
      }
      _condVar.notify_one();
   }

   /**
    * @brief Register a listener callback for new data.
    *
    * @param listener Callback invoked (on the worker thread) for each item.
    * @return A unique registration ID, or -1 if the handler is stopped.
    */
   int registerListener(const Listener& listener)
   {
      if (_stopFlag) return -1;
      const std::lock_guard<std::mutex> lock(_listenersMutex);
      _nextListenerId++;
      _listeners[_nextListenerId] = listener;
      return _nextListenerId;
   }

   /**
    * @brief Unregister a listener by its registration ID.
    *
    * @param id The registration ID returned by registerListener().
    */
   void unregisterListener(int id)
   {
      if (_stopFlag) return;
      const std::lock_guard<std::mutex> lock(_listenersMutex);
      _listeners.erase(id);
   }

   /**
    * @brief Get usage statistics.
    *
    * @return Pair of (listener count, queued item count).
    */
   std::pair<size_t, size_t> watermarkInfo()
   {
      if (_stopFlag) return {0, 0};

      const std::lock_guard<std::mutex> lock(_listenersMutex);
      return {_listeners.size(), _dataQueue.size()};
   }

private:
   void processData()
   {
      while (!_stopFlag)
      {
         T data;
         {
            std::unique_lock<std::mutex> lock(_cvMutex);
            _condVar.wait(lock, [this] { return !_dataQueue.empty() || _stopFlag; });
            if (_stopFlag && _dataQueue.empty())
            {
               return;
            }
            data = _dataQueue.front();
            _dataQueue.pop();
         }
         notifyListeners(data);
      }
   }

   void notifyListeners(const T& data)
   {
      const std::lock_guard<std::mutex> lock(_listenersMutex);
      for (const auto& listener : _listeners)
      {
         try
         {
            listener.second(data);
         }
         catch (const std::exception& e)
         {
            std::cerr << "Listener threw an std::exception! " << e.what() << '\n';
         }
         catch (...)
         {
            std::cerr << "Listener threw an unknown exception!\n";
         }
      }
   }

   std::mutex _listenersMutex;
   std::map<int, Listener> _listeners;
   int _nextListenerId = 123;
   std::queue<T> _dataQueue;
   std::mutex _cvMutex;
   std::condition_variable _condVar;
   std::thread _workerThread;
   std::atomic<bool> _stopFlag;
};

} // namespace CommonUtils

#endif // COMMONUTILS_DATAHANDLER_H_
