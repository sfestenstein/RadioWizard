#ifndef ZYRELIB_ZYRENODE_H_
#define ZYRELIB_ZYRENODE_H_

// System headers
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

// Third-party headers
#include <zyre.h>

/**
 * @class ZyreNode
 * @brief Base class for Zyre-based peer-to-peer messaging nodes.
 *
 * Manages the lifecycle of a Zyre node: creation, start, and stop.
 * Derived classes (ZyrePublisher, ZyreSubscriber) add publish/subscribe
 * behaviour on top of this base.
 */
class ZyreNode
{
public:
   /**
    * @brief Construct a Zyre node.
    *
    * @param name           Namespace name for topic isolation.
    * @param interfaceAddr  Network interface to use.  Accepts either an
    *                       interface name (e.g. "eth0") or a local IP address.
    *                       Pass "" to let Zyre auto-detect the interface.
    */
   explicit ZyreNode(const std::string& name,
                     const std::string& interfaceAddr = "");

   /**
    * @brief Destroy the node.
    */
   virtual ~ZyreNode();

   // Non-copyable.
   ZyreNode(const ZyreNode&) = delete;
   ZyreNode& operator=(const ZyreNode&) = delete;

   /**
    * @brief Start the Zyre node.
    *
    * @return true on success.
    */
   bool start();

   /**
    * @brief Request the node to stop (calls zyre_stop).
    */
   void stop();

   /**
    * @brief Get the node namespace name.
    *
    * @return The namespace name.
    */
   [[nodiscard]] const std::string& name() const { return _nodeName; }

protected:
   zyre_t* _node;
   std::string _nodeName;

   // Running flag that derived classes should check.
   std::atomic<bool> _isRunning{true};

private:
   std::mutex _terminateMutex;
   std::condition_variable _terminateCV;
   bool _stopRequested{false};
};

#endif // ZYRELIB_ZYRENODE_H_