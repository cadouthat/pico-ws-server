cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

include(../pico_sdk_import.cmake)

project(ws_server_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(STATIC_HTML_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(STATIC_HTML_FILENAME "static.html")
add_subdirectory(.. pico-ws-server)

add_library(lwipopts_provider INTERFACE)
target_include_directories(lwipopts_provider INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

add_executable(example
  example.cpp
)

pico_enable_stdio_usb(example 1)

target_link_libraries(example
  pico_stdlib
  pico_cyw43_arch_lwip_poll
  pico_ws_server
)

target_compile_definitions(example PRIVATE
  WIFI_SSID=\"${WIFI_SSID}\"
  WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
  )

pico_add_extra_outputs(example)
