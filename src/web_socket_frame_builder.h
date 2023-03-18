#ifndef __WEB_SOCKET_FRAME_BUILDER_H__
#define __WEB_SOCKET_FRAME_BUILDER_H__

#include <cstddef>
#include <memory>
#include <stdint.h>

#include "web_socket_frame.h"

class WebSocketMessageBuilder;

class WebSocketFrameBuilder {
 public:
  static constexpr auto MAX_HEADER_SIZE = 14;

  explicit WebSocketFrameBuilder(WebSocketMessageBuilder& message_builder)
      : message_builder(message_builder) {}

  bool process(uint8_t byte);

  size_t makeHeader(bool final, uint8_t opcode, size_t payload_size, uint8_t header_out[MAX_HEADER_SIZE]);

 private:
  static constexpr auto MAX_PAYLOAD_SIZE = 64000;

  WebSocketMessageBuilder& message_builder;

  uint8_t header[MAX_HEADER_SIZE];
  size_t header_bytes = 0;

  std::unique_ptr<WebSocketFrame> frame;

  bool isHeaderComplete();
};

#endif
