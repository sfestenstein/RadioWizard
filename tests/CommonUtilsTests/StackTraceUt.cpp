#include <gtest/gtest.h>

#include "StackTrace.h"

#include <csignal>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

// ============================================================================
// captureStackTrace() — non-signal path
// ============================================================================

TEST(StackTraceTest, CaptureStackTrace_ReturnsNonEmptyString)
{
   const std::string trace = CommonUtils::StackTrace::captureStackTrace();
   EXPECT_FALSE(trace.empty());
}

TEST(StackTraceTest, CaptureStackTrace_ContainsFrameNumbers)
{
   const std::string trace = CommonUtils::StackTrace::captureStackTrace();
   // The output should contain at least frame "0#"
   EXPECT_NE(trace.find("0#"), std::string::npos);
}

TEST(StackTraceTest, CaptureStackTrace_SkipFramesReducesOutput)
{
   const std::string full    = CommonUtils::StackTrace::captureStackTrace(0);
   const std::string skipped = CommonUtils::StackTrace::captureStackTrace(3);

   // Skipping more frames should produce a shorter (or equal) result.
   EXPECT_LE(skipped.size(), full.size());
}

// ============================================================================
// installSignalHandlers() — idempotency
// ============================================================================

TEST(StackTraceTest, InstallSignalHandlers_CanBeCalledMultipleTimes)
{
   // Should not throw or crash when called repeatedly.
   CommonUtils::StackTrace::installSignalHandlers();
   CommonUtils::StackTrace::installSignalHandlers();
   SUCCEED();
}

// ============================================================================
// Signal handler integration — fork + wait to avoid killing the test runner
// ============================================================================

TEST(StackTraceTest, SignalHandler_CatchesSigsegv)
{
   // Fork so the segfault doesn't kill the test process.
   const pid_t pid = fork();
   ASSERT_NE(pid, -1) << "fork() failed";

   if (pid == 0)
   {
      // Child: install handler and trigger SIGSEGV.
      CommonUtils::StackTrace::installSignalHandlers();

      // Redirect stderr to /dev/null so the stack trace banner doesn't
      // clutter test output.
      const int devNull = open("/dev/null", O_WRONLY);
      if (devNull != -1)
      {
         dup2(devNull, STDERR_FILENO);
         if (close(devNull) != 0)
         {
            _exit(3);
         }
      }

      if (raise(SIGSEGV) != 0)
      {
         _exit(2);
      }

      // Should not reach here.
      _exit(0);
   }

   // Parent: wait for the child and verify it was killed by SIGSEGV.
   int status = 0;
   waitpid(pid, &status, 0);

   EXPECT_TRUE(WIFSIGNALED(status));
   EXPECT_EQ(WTERMSIG(status), SIGSEGV);
}

// ============================================================================
// Post-crash hook — verify the hook fires before the process terminates
// ============================================================================

TEST(StackTraceTest, PostCrashHook_IsCalledOnSignal)
{
   // Use a pipe to communicate from the child's hook back to the parent.
   int pipeFds[2];
   ASSERT_EQ(pipe(pipeFds), 0);

   const pid_t pid = fork();
   ASSERT_NE(pid, -1) << "fork() failed";

   if (pid == 0)
   {
      // Child: close read end, install a hook that writes to the pipe,
      // then trigger SIGSEGV.
      if (close(pipeFds[0]) != 0)
      {
         _exit(3);
      }

      CommonUtils::StackTrace::setPostCrashHook([fd = pipeFds[1]](int sig)
      {
         // Write the signal number as a single byte marker.
         const auto byte = static_cast<char>(sig);
         (void)write(fd, &byte, 1);
         if (close(fd) != 0)
         {
            _exit(4);
         }
      });
      CommonUtils::StackTrace::installSignalHandlers();

      // Suppress stderr output.
      const int devNull = open("/dev/null", O_WRONLY);
      if (devNull != -1)
      {
         dup2(devNull, STDERR_FILENO);
         if (close(devNull) != 0)
         {
            _exit(5);
         }
      }

      if (raise(SIGSEGV) != 0)
      {
         _exit(2);
      }
      _exit(0);
   }

   // Parent: close write end, read from pipe.
   ASSERT_EQ(close(pipeFds[1]), 0);

   char buf = 0;
   const ssize_t n = read(pipeFds[0], &buf, 1);
   ASSERT_EQ(close(pipeFds[0]), 0);

   int status = 0;
   waitpid(pid, &status, 0);

   // The hook should have written exactly 1 byte with the signal value.
   EXPECT_EQ(n, 1);
   EXPECT_EQ(static_cast<int>(buf), SIGSEGV);
   EXPECT_TRUE(WIFSIGNALED(status));
}
