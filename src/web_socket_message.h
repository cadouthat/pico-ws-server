#ifndef __WEB_SOCKET_MESSAGE_H__
#define __WEB_SOCKET_MESSAGE_H__

#include <cstddef>
#include <list>
#include <memory>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "web_socket_frame.h"

class WebSocketMessage {
 public:
  enum Type : uint8_t {
    UNKNOWN,
    TEXT = 0x01,
    BINARY = 0x02,
    CLOSE = 0x08,
    PING = 0x09,
    PONG = 0x0A,
  };

  WebSocketMessage(Type type, const void* payload, size_t payload_size)
      : type(type), const_payload((uint8_t*)payload), payload_size(payload_size) {}

  explicit WebSocketMessage(const std::list<std::unique_ptr<WebSocketFrame>>& frames) {
    for (const auto& frame : frames) {
      payload_size += frame->getPayloadSize();
    }

    payload.resize(payload_size + 1);

    size_t payload_filled = 0;
    for (const auto& frame : frames) {
      Type frame_type = opcodeToType(frame->getOpcode());
      if (frame_type != UNKNOWN) {
        type = frame_type;
      }

      memcpy(&payload[payload_filled], frame->getPayload(), frame->getPayloadSize());
      payload_filled += frame->getPayloadSize();
    }

    // Null terminator to allow payloads to be treated as strings
    payload[payload_size] = 0;
  }

  Type getType() const {
    return type;
  }

  size_t getPayloadSize() const {
    return payload_size;
  }
  const uint8_t* getPayload() const {
    if (const_payload) {
      return const_payload;
    }
    return payload.data();
  }

 private:
  Type type = UNKNOWN;
  size_t payload_size = 0;
  std::vector<uint8_t> payload;
  const uint8_t* const_payload = nullptr;

  Type opcodeToType(uint8_t opcode) {
    switch (opcode) {
    case TEXT:
      return TEXT;
    case BINARY:
      return BINARY;
    case CLOSE:
      return CLOSE;
    case PING:
      return PING;
    case PONG:
      return PONG;
    default:
      return UNKNOWN;
    }
  }
};

#endif
