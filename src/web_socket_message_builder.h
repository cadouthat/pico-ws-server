#ifndef __WEB_SOCKET_MESSAGE_BUILDER_H__
#define __WEB_SOCKET_MESSAGE_BUILDER_H__

#include <cstddef>
#include <list>
#include <memory>
#include <stdint.h>

#include "web_socket_frame.h"
#include "web_socket_frame_builder.h"
#include "web_socket_message.h"

class WebSocketHandler;

class WebSocketMessageBuilder {
 public:
  WebSocketMessageBuilder(WebSocketHandler& handler)
      : handler(handler), frame_builder(*this) {}

  bool process(uint8_t byte);

  bool processFrame(std::unique_ptr<WebSocketFrame> frame);

  bool sendMessage(const WebSocketMessage& message);

 private:
  static constexpr auto MAX_MESSAGE_FRAMES = 1024;
  static constexpr auto MAX_MESSAGE_SIZE = 64000;

  WebSocketHandler& handler;
  WebSocketFrameBuilder frame_builder;
  std::list<std::unique_ptr<WebSocketFrame>> message_frames;
  size_t total_message_size = 0;
};

#endif
