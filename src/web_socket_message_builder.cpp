#include "web_socket_message_builder.h"

#include <memory>
#include <new>
#include <cstring>
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
    return handler.processMessage(std::move(message));
  }

  return true;
}

bool WebSocketMessageBuilder::sendMessage(const WebSocketMessage& message) {
  const size_t payload_size = message.getPayloadSize();

  uint8_t header[WebSocketFrameBuilder::MAX_HEADER_SIZE];
  const size_t header_len = frame_builder.makeHeader(/*final=*/true, message.getType(), payload_size, header);
  const size_t frame_size = header_len + payload_size;

  const uint8_t* payload = message.getPayload();

  // Pack header + payload contiguously so tcp_write sees one frame and avoids partial message errors.
  uint8_t stack_buffer[WebSocketFrameBuilder::MAX_HEADER_SIZE + 256];
  uint8_t* frame_data = stack_buffer;
  std::unique_ptr<uint8_t[]> heap_buffer;
  if (frame_size > sizeof(stack_buffer)) {
    heap_buffer.reset(new (std::nothrow) uint8_t[frame_size]);
    if (!heap_buffer) {
      return false;
    }
    frame_data = heap_buffer.get();
  }

  std::memcpy(frame_data, header, header_len);
  if (payload_size > 0 && payload) {
    std::memcpy(frame_data + header_len, payload, payload_size);
  }

  if (!handler.sendRaw(frame_data, frame_size)) {
    return false;
  }

  if (!handler.flushSend()) {
    // TODO: does flushing need to be re-attempted later?
    DEBUG("flushSend failed");
  }
  return true;
}
