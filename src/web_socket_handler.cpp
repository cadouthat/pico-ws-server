#include "web_socket_handler.h"

#include <cstddef>

#include "lwip/pbuf.h"

#include "client_connection.h"
#include "debug.h"
#include "web_socket_message.h"

bool WebSocketHandler::process(struct pbuf* pb) {
  if (is_closing) {
    return false;
  }

  for (size_t i = 0; i < pb->tot_len; i++) {
    char c = pbuf_get_at(pb, i);
    if (!message_builder.process(c)) {
      if (!is_closing) {
        sendMessage(WebSocketMessage(WebSocketMessage::CLOSE, nullptr, 0));
        is_closing = true;
      }
      return false;
    }
  }
  return true;
}

bool WebSocketHandler::sendRaw(const void* data, size_t size) {
  return connection.sendRaw(data, size);
}

bool WebSocketHandler::processMessage(const WebSocketMessage& message) {
  switch (message.getType()) {
  case WebSocketMessage::TEXT:
  case WebSocketMessage::BINARY:
    connection.processWebSocketMessage(message.getPayload(), message.getPayloadSize());
    return true;

  case WebSocketMessage::CLOSE:
    DEBUG("CLOSE requested");
    sendMessage(message);
    is_closing = true;
    return false;

  case WebSocketMessage::PING:
    DEBUG("PING requested");
    return sendMessage(message);

  default:
    // Ignore unknown message types
    DEBUG("unknown message type");
    return true;
  }
}

bool WebSocketHandler::sendMessage(const WebSocketMessage& message) {
  if (is_closing) {
    return false;
  }

  return message_builder.sendMessage(message);
}