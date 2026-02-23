#ifndef STACKTRACE_H_
#define STACKTRACE_H_

/**
 * @file StackTrace.h
 * @brief POSIX signal handler that prints a demangled stack trace on crashes.
 *
 * Installs a signal handler for SIGSEGV, SIGABRT, and SIGBUS that uses a
 * two-phase approach:
 *
 *  **Phase 1 (async-signal-safe):** Writes a stack trace banner and raw
 *  frame addresses to STDERR_FILENO using only write() and
 *  backtrace_symbols_fd().  This always succeeds even if the heap is
 *  corrupted.
 *
 *  **Phase 2 (best-effort):** Attempts to log the crash via spdlog,
 *  dump the trace ring-buffer, and flush all log files.  This is NOT
 *  async-signal-safe and may deadlock if the crash occurred inside
 *  malloc or spdlog itself — but Phase 1 output is already on stderr.
 *  An alarm(3) watchdog ensures the process terminates even if Phase 2
 *  hangs.
 *
 *  **Phase 3:** Re-raises the original signal with the default
 *  disposition so the OS can generate a core dump.
 */

#include <csignal>
#include <functional>
#include <string>
#include <string_view>

namespace CommonUtils
{

/**
 * @class StackTrace
 * @brief Installs POSIX signal handlers that dump a stack trace on crash.
 *
 * Typical usage — call once early in main(), after logger init:
 * @code
 * CommonUtils::GeneralLogger logger;
 * logger.init("myapp");
 *
 * CommonUtils::StackTrace::setPostCrashHook([](int sig)
 * {
 *    GPCRIT("Caught signal {} — see stderr for stack trace", sig);
 *    if (CommonUtils::GeneralLogger::s_traceLogger)
 *    {
 *       CommonUtils::GeneralLogger::s_traceLogger->dump_backtrace();
 *       CommonUtils::GeneralLogger::s_traceLogger->flush();
 *    }
 *    if (CommonUtils::GeneralLogger::s_generalLogger)
 *    {
 *       CommonUtils::GeneralLogger::s_generalLogger->flush();
 *    }
 * });
 *
 * CommonUtils::StackTrace::installSignalHandlers();
 * @endcode
 */
class StackTrace
{
public:
   /** @brief Callback type invoked after the safe Phase 1 output. */
   using PostCrashHook = std::function<void(int /*signal*/)>;

   /**
    * @brief Install crash signal handlers.
    *
    * Registers a handler for SIGSEGV, SIGABRT, and SIGBUS.
    * Safe to call more than once — subsequent calls are no-ops.
    */
   static void installSignalHandlers();

   /**
    * @brief Register a best-effort callback for Phase 2.
    *
    * The hook is called from inside the signal handler after the
    * async-signal-safe stack trace has been written to stderr.
    * It is wrapped in a try/catch and guarded by an alarm()
    * watchdog, so a hang or throw will not prevent process
    * termination.
    *
    * @param hook Callable taking the signal number.
    */
   static void setPostCrashHook(PostCrashHook hook);

   /**
    * @brief Obtain a formatted stack trace string (non-signal-safe).
    *
    * Useful for logging from normal (non-signal) contexts, e.g. on
    * caught exceptions.  NOT safe to call from a signal handler.
    *
    * @param skipFrames Number of innermost frames to skip (default 1
    *                   to omit captureStackTrace itself).
    * @return Multi-line string with one frame per line.
    */
   [[nodiscard]] static std::string captureStackTrace(int skipFrames = 1);

private:
   /** @brief The signal handler installed for crash signals. */
   static void signalHandler(int signal);

   /**
    * @brief Write a C string to STDERR_FILENO (async-signal-safe).
    * @param msg Null-terminated string to write.
    */
   static void writeStderr(const char* msg);

   /** @brief Maximum number of stack frames to capture. */
   static constexpr int MAX_FRAMES = 128;

   /** @brief Seconds before alarm() kills the process if Phase 2 hangs. */
   static constexpr unsigned int PHASE2_TIMEOUT_SEC = 3;

   /** @brief Guards against multiple installations. */
   static bool s_installed; // NOLINT(readability-identifier-naming)

   /** @brief Optional user-supplied hook for Phase 2 (logger flush, etc.). */
   static PostCrashHook s_postCrashHook; // NOLINT(readability-identifier-naming)
};

} // namespace CommonUtils

#endif // STACKTRACE_H_
