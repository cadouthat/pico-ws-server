#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

#include <cstddef>

class HTTPRequest {
 public:
  enum ProcessResult {
    CONTINUE,
    BAD_REQUEST,
    BAD_METHOD,
    NOT_FOUND,
    ATTEMPT_UPGRADE,
  };

  HTTPRequest() {}
  ~HTTPRequest() {}
 
  ProcessResult process(struct pbuf* pb);

 private:
  friend class HTTPHandler;

  static constexpr auto HEADER_BUF_SIZE = 64;

  enum RequestPart {
    METHOD,
    PATH,
    PROTOCOL,
    HEADER,
    LINE_DELIM,
  };

  size_t request_bytes = 0;
  RequestPart current_part = METHOD;
  size_t current_index = 0;

  char current_header[HEADER_BUF_SIZE] = {0};

  bool has_upgrade_header = false;
  bool has_connection_header = false;
  bool has_ws_version_header = false;
  char ws_key_header_value[HEADER_BUF_SIZE] = {0};

  bool matchAndThen(char c, const char* expected, RequestPart next_part);
  void processHeader(const char* header);
  ProcessResult process(char c);
};

#endif
