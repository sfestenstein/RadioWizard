#ifndef SNOOZABLETIMER_H_
#define SNOOZABLETIMER_H_

// System headers
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

/**
 * @class SnoozableTimer
 * @brief Timer that executes a callback after a snooze period, with the
 *        ability to extend (snooze) the deadline.
 *
 * Once started, the timer counts down for the configured snooze period.
 * Calling snooze() resets the deadline to NOW + snoozePeriod.
 */
class SnoozableTimer
{
public:
   /**
    * @brief Construct a SnoozableTimer.
    *
    * @param function       Callback to invoke when the timer expires.
    * @param snoozePeriodMs Initial snooze period in milliseconds.
    */
   SnoozableTimer(std::function<void()> function, int snoozePeriodMs);

   /**
    * @brief Destroy the timer, stopping it if running.
    */
   ~SnoozableTimer();

   // Non-copyable.
   SnoozableTimer(const SnoozableTimer&) = delete;
   SnoozableTimer& operator=(const SnoozableTimer&) = delete;

   /**
    * @brief Start the timer.
    */
   void start();

   /**
    * @brief Stop the timer.
    */
   void stop();

   /**
    * @brief Reset the deadline to NOW + snoozePeriod.
    */
   void snooze();

   /**
    * @brief Update the snooze period and immediately snooze with the new
    *        duration.
    *
    * @param snoozePeriodMs New snooze period in milliseconds.
    */
   void updateSnoozePeriod(int snoozePeriodMs);

private:
   std::function<void()> _function;
   std::chrono::high_resolution_clock::time_point _executionTime;
   mutable std::mutex _mutex;
   std::condition_variable _cv;
   std::thread _thread;
   int _snoozePeriodMs;
   bool _isRunning = false;
};

#endif // SNOOZABLETIMER_H_
