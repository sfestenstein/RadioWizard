#ifndef ZYREPUBLISHER_H_
#define ZYREPUBLISHER_H_

// Project headers
#include "ZyreNode.h"

// Third-party headers
#include <google/protobuf/message.h>

/**
 * @class ZyrePublisher
 * @brief Zyre-based publisher that sends protobuf messages to a peer group.
 */
class ZyrePublisher : public ZyreNode
{
public:
   /**
    * @brief Construct a Zyre publisher.
    *
    * @param name           Namespace name for topic isolation.
    * @param interfaceAddr  Network interface to use (interface name or local
    *                       IP).  Pass "" to let Zyre auto-detect.
    */
   explicit ZyrePublisher(const std::string& name,
                          const std::string& interfaceAddr = "");

   /**
    * @brief Destroy the publisher.
    */
   ~ZyrePublisher();

   /**
    * @brief Publish a protobuf message to the specified topic.
    *
    * @param topic   Topic string (prefixed with the namespace).
    * @param message Protobuf message to serialize and send.
    * @return true on success, false on failure.
    */
   bool publish(const std::string& topic,
                const google::protobuf::Message& message);
};

#endif // ZYREPUBLISHER_H_
