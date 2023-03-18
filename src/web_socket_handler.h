#ifndef __WEB_SOCKET_HANDLER_H__
#define __WEB_SOCKET_HANDLER_H__

#include <cstddef>

#include "lwip/pbuf.h"

#include "web_socket_message.h"
#include "web_socket_message_builder.h"

class ClientConnection;

class WebSocketHandler {
 public:
  explicit WebSocketHandler(ClientConnection& connection)
      : connection(connection), message_builder(*this) {}

  bool process(struct pbuf* pb);
  bool sendRaw(const void* data, size_t size);

  bool processMessage(const WebSocketMessage& message);

  bool sendMessage(const WebSocketMessage& message);

  bool isClosing() { return is_closing; }

 private:
  ClientConnection& connection;
  WebSocketMessageBuilder message_builder;
  bool is_closing = false;
};

#endif
