#include "web_socket_message_builder.h"

#include <memory>
#include <stdint.h>

#include "debug.h"
#include "web_socket_frame.h"
#include "web_socket_frame_builder.h"
#include "web_socket_handler.h"

bool WebSocketMessageBuilder::process(uint8_t byte) {
  return frame_builder.process(byte);
}

bool WebSocketMessageBuilder::processFrame(std::unique_ptr<WebSocketFrame> frame) {
  if (message_frames.size() >= MAX_MESSAGE_FRAMES) {
    return false;
  }
  if (total_message_size + frame->getPayloadSize() > MAX_MESSAGE_SIZE) {
    return false;
  }

  total_message_size += frame->getPayloadSize();
  message_frames.push_back(std::move(frame));

  if (message_frames.back()->isFinal()) {
    // Reset and process completed message
    WebSocketMessage message(message_frames);
    message_frames.clear();
    total_message_size = 0;
    return handler.processMessage(message);
  }

  return true;
}

bool WebSocketMessageBuilder::sendMessage(const WebSocketMessage& message) {
  uint8_t header[WebSocketFrameBuilder::MAX_HEADER_SIZE];
  size_t header_len = frame_builder.makeHeader(/*final=*/true, message.getType(), message.getPayloadSize(), header);
  if (!handler.sendRaw(header, header_len)) {
    return false;
  }

  if (!handler.sendRaw(message.getPayload(), message.getPayloadSize())) {
    return false;
  }

  if (!handler.flushSend()) {
    // TODO: does flushing need to be re-attempted later?
    DEBUG("flushSend failed");
  }
  return true;
}
