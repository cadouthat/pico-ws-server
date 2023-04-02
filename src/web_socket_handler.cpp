#include "web_socket_handler.h"

#include <cstddef>

#include "lwip/pbuf.h"

#include "client_connection.h"
#include "debug.h"
#include "web_socket_message.h"

bool WebSocketHandler::process(struct pbuf* pb) {
  for (size_t i = 0; i < pb->tot_len; i++) {
    char c = pbuf_get_at(pb, i);
    if (!message_builder.process(c)) {
      // Attempt a graceful disconnect, but set is_closing regardless
      close();
      is_closing = true;
      return false;
    }
  }
  return true;
}

bool WebSocketHandler::sendRaw(const void* data, size_t size) {
  return connection.sendRaw(data, size);
}

bool WebSocketHandler::flushSend() {
  return connection.flushSend();
}

bool WebSocketHandler::processMessage(const WebSocketMessage& message) {
  switch (message.getType()) {
  case WebSocketMessage::TEXT:
  case WebSocketMessage::BINARY:
    connection.processWebSocketMessage(message.getPayload(), message.getPayloadSize());
    return true;

  case WebSocketMessage::CLOSE:
    if (!is_closing) {
      DEBUG("CLOSE requested");
      sendMessage(message);
      is_closing = true;
    }
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

bool WebSocketHandler::close() {
  if (is_closing) {
    return true;
  }

  if (!message_builder.sendMessage(WebSocketMessage(WebSocketMessage::CLOSE, nullptr, 0))) {
    return false;
  }

  is_closing = true;
  return true;
}
