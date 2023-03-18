cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_ws_server C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_library(pico_ws_server
  src/client_connection.cpp
  src/http_handler.cpp
  src/web_socket_frame_builder.cpp
  src/web_socket_handler.cpp
  src/web_socket_message_builder.cpp
  src/web_socket_server.cpp
  src/web_socket_server_internal.cpp
)

target_include_directories(pico_ws_server PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include )
target_include_directories(pico_ws_server PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src )

target_link_libraries(pico_ws_server
  pico_stdlib
  pico_cyw43_driver
  pico_lwip_nosys
  pico_mbedtls
)

if (DEFINED PICO_WS_SERVER_STATIC_HTML)
  string(REPLACE "\\" "\\\\" PICO_WS_SERVER_STATIC_HTML_ESCAPED ${PICO_WS_SERVER_STATIC_HTML})
  string(REPLACE "\n" "\\n" PICO_WS_SERVER_STATIC_HTML_ESCAPED ${PICO_WS_SERVER_STATIC_HTML_ESCAPED})
  string(REPLACE "\"" "\\\"" PICO_WS_SERVER_STATIC_HTML_ESCAPED ${PICO_WS_SERVER_STATIC_HTML_ESCAPED})
  target_compile_definitions(pico_ws_server PRIVATE
    PICO_WS_SERVER_STATIC_HTML="${PICO_WS_SERVER_STATIC_HTML_ESCAPED}"
  )
endif()
