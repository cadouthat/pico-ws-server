#include "web_socket_frame_builder.h"

#include <limits.h>
#include <memory>
#include <stdint.h>

#include "web_socket_message_builder.h"

namespace {

constexpr auto MIN_HEADER_LEN = 2;

constexpr auto FIN_BIT_MASK = 1 << 7;
constexpr auto OPCODE_MASK = 0b1111;

constexpr auto MASK_BIT_MASK = 1 << 7;
constexpr auto PRE_LEN_MASK = ~MASK_BIT_MASK;

constexpr auto PRE_LEN_MAX = 125;
constexpr auto PRE_LEN_EX2 = 126;
constexpr auto PRE_LEN_EX8 = 127;

uint16_t read_u16_big_endian(uint8_t* source) {
  return ((uint16_t)source[0] << 8) | (uint16_t)source[1];
}

uint32_t read_u32_big_endian(uint8_t* source) {
  return
    ((uint32_t)source[0] << 24) |
    ((uint32_t)source[1] << 16) |
    ((uint32_t)source[2] << 8) |
    ((uint32_t)source[3]);
}

uint64_t read_u64_big_endian(uint8_t* source) {
  return
    ((uint64_t)source[0] << 56) |
    ((uint64_t)source[1] << 48) |
    ((uint64_t)source[2] << 40) |
    ((uint64_t)source[3] << 32) |
    ((uint64_t)source[4] << 24) |
    ((uint64_t)source[5] << 16) |
    ((uint64_t)source[6] << 8) |
    ((uint64_t)source[7]);
}

void write_u16_big_endian(uint16_t source, uint8_t* dest) {
  dest[0] = (source >> 8) & 0xFF;
  dest[1] = (source) & 0xFF;
}

void write_u64_big_endian(uint64_t source, uint8_t* dest) {
  dest[0] = (source >> 56) & 0xFF;
  dest[1] = (source >> 48) & 0xFF;
  dest[2] = (source >> 40) & 0xFF;
  dest[3] = (source >> 32) & 0xFF;
  dest[4] = (source >> 24) & 0xFF;
  dest[5] = (source >> 16) & 0xFF;
  dest[6] = (source >> 8) & 0xFF;
  dest[7] = (source) & 0xFF;
}

uint8_t getOpcode(uint8_t* header) {
  return header[0] & OPCODE_MASK;
}

bool isFinal(uint8_t* header) {
  return header[0] & FIN_BIT_MASK;
}

uint8_t getPreLen(uint8_t* header) {
  return header[1] & PRE_LEN_MASK;
}

bool isMasked(uint8_t* header) {
  return header[1] & MASK_BIT_MASK;
}

uint32_t getMask(uint8_t* header) {
  if (!isMasked(header)) {
    return 0;
  }

  size_t offset = MIN_HEADER_LEN;
  uint8_t pre_len = getPreLen(header);
  if (pre_len == PRE_LEN_EX2) {
    offset += 2;
  }
  if (pre_len == PRE_LEN_EX8) {
    offset += 8;
  }

  return read_u32_big_endian(&header[offset]);
}

size_t getPayloadSize(uint8_t* header, size_t max) {
  uint64_t len = getPreLen(header);

  if (len == PRE_LEN_EX2) {
    len = read_u16_big_endian(&header[2]);
  } else if (len == PRE_LEN_EX8) {
    len = read_u64_big_endian(&header[2]);
  }

  if (len > max) {
    return SIZE_MAX;
  }
  return len;
}

} // namespace

bool WebSocketFrameBuilder::process(uint8_t byte) {
  bool consumed = false;
  if (!isHeaderComplete()) {
    header[header_bytes++] = byte;
    consumed = true;
    if (!isHeaderComplete()) {
      return true;
    }
  }

  if (!isMasked(header)) {
    // Frames from client must be masked
    return false;
  }
  size_t payload_size = getPayloadSize(header, MAX_PAYLOAD_SIZE);
  if (payload_size == SIZE_MAX) {
    // Unsupported payload size
    return false;
  }

  if (!frame) {
    frame = std::make_unique<WebSocketFrame>(getOpcode(header), isFinal(header), payload_size);
  }

  if (!consumed) {
    frame->append(byte);
    consumed = true;
  }

  if (frame->getPayloadSize() < payload_size) {
    return true;
  }

  frame->maskPayload(getMask(header));

  // Reset and process completed frame
  header_bytes = 0;
  return message_builder.processFrame(std::move(frame));
}

size_t WebSocketFrameBuilder::makeHeader(bool final, uint8_t opcode, size_t payload_size, uint8_t header_out[MAX_HEADER_SIZE]) {
  size_t header_len = MIN_HEADER_LEN;
  header_out[0] = (final ? FIN_BIT_MASK : 0) | (opcode & OPCODE_MASK);

  if (payload_size > UINT16_MAX) {
    header_out[1] = PRE_LEN_EX8;
    write_u64_big_endian(payload_size, &header_out[2]);
    header_len += 8;
  } else if (payload_size > PRE_LEN_MAX) {
    header_out[1] = PRE_LEN_EX2;
    write_u16_big_endian(payload_size, &header_out[2]);
    header_len += 2;
  } else {
    header_out[1] = payload_size;
  }

  return header_len;
}

bool WebSocketFrameBuilder::isHeaderComplete() {
  size_t bytes_needed = MIN_HEADER_LEN;
  if (header_bytes < bytes_needed) {
    return false;
  }

  uint8_t pre_len = getPreLen(header);
  if (pre_len == PRE_LEN_EX2) {
    bytes_needed += 2;
  }
  if (pre_len == PRE_LEN_EX8) {
    bytes_needed += 8;
  }

  if (isMasked(header)) {
    bytes_needed += 4;
  }

  return header_bytes >= bytes_needed;
}
