#ifndef COMMONUTILS_TIMER_H_
#define COMMONUTILS_TIMER_H_

// System headers
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace CommonUtils
{

/**
 * @class Timer
 * @brief Simple timer that can execute a function after a specified amount
 *        of time, or periodically.  Designed for easy cancellation and
 *        destruction.
 */
class Timer
{
public:
   /**
    * @brief Construct a cancelable timer.
    */
   Timer();

   /**
    * @brief Destroy the timer, cancelling any pending operation.
    */
   ~Timer();

   // Non-copyable.
   Timer(const Timer&) = delete;
   Timer& operator=(const Timer&) = delete;

   /**
    * @brief Execute a function once after a specified delay.
    *
    * @param func     Function to execute.
    * @param interval Delay before execution, in milliseconds.
    */
   void startOneShot(const std::function<void()>& func, unsigned int interval);

   /**
    * @brief Execute a function periodically.
    *
    * @param func     Function to execute.
    * @param interval Period between executions, in milliseconds.
    */
   void startPeriodic(const std::function<void()>& func, unsigned int interval);

   /**
    * @brief Cancel the timer.
    */
   void stop();

private:
   std::atomic<bool> _isRunning;
   std::thread _thread;
   std::mutex _mutex;
   std::condition_variable _cv;
};

} // namespace CommonUtils

#endif // COMMONUTILS_TIMER_H_
