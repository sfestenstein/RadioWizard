#ifndef ZYRESUBSCRIBER_H_
#define ZYRESUBSCRIBER_H_

// Project headers
#include "ZyreNode.h"

// System headers
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

/**
 * @class ZyreSubscriber
 * @brief Zyre-based subscriber that receives protobuf messages from peers.
 *
 * Runs a background receive loop and dispatches incoming messages to
 * per-topic handler callbacks.
 */
class ZyreSubscriber : public ZyreNode
{
public:
   /**
    * @brief Callback type: receives the raw message data as a string.
    */
   using MessageHandler = std::function<void(const std::string& topic,
                                             const std::string& data)>;

   /**
    * @brief Construct a Zyre subscriber.
    *
    * @param name           Namespace name for topic isolation.
    * @param interfaceAddr  Network interface to use (interface name or local
    *                       IP).  Pass "" to let Zyre auto-detect.
    */
   explicit ZyreSubscriber(const std::string& name,
                           const std::string& interfaceAddr = "");

   /**
    * @brief Destroy the subscriber, stopping the receive loop.
    */
   ~ZyreSubscriber();

   /**
    * @brief Subscribe to a topic with a handler callback.
    *
    * Can be called at any time while the subscriber is running.
    *
    * @param topic   Topic name to subscribe to.
    * @param handler Callback invoked when a message arrives on this topic.
    */
   void subscribe(const std::string& topic, MessageHandler handler);

private:
   void receiveLoop();

   std::unordered_map<std::string, MessageHandler> _handlers;
   std::mutex _handlersMutex;
   std::thread _receiveThread;
};

#endif // ZYRESUBSCRIBER_H_