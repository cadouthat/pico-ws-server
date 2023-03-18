#ifndef __WEB_SOCKET_FRAME_H__
#define __WEB_SOCKET_FRAME_H__

#include <cstddef>
#include <vector>

class WebSocketFrame {
 public:
  WebSocketFrame(uint8_t opcode, bool final, size_t capacity) : opcode(opcode), final(final) {
    payload.reserve(capacity);
  }

  uint8_t getOpcode() {
    return opcode;
  }
  bool isFinal() {
    return final;
  }

  void append(uint8_t byte) {
    payload.push_back(byte);
  }

  void maskPayload(uint32_t mask) {
    uint8_t big_endian[4];
    big_endian[0] = (mask >> 24) & 0xFF;
    big_endian[1] = (mask >> 16) & 0xFF;
    big_endian[2] = (mask >> 8) & 0xFF;
    big_endian[3] = mask & 0xFF;
    for (size_t i = 0; i < payload.size(); i++) {
      payload[i] ^= big_endian[i % 4];
    }
  }

  size_t getPayloadSize() {
    return payload.size();
  }
  const uint8_t* getPayload() {
    return payload.data();
  }

 private:
  uint8_t opcode;
  bool final;
  std::vector<uint8_t> payload;
};

#endif
