cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_ws_server C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(STATIC_HTML_PATH "" CACHE PATH "Directory of static HTML file" PARENT_SCOPE)
set(STATIC_HTML_FILENAME "" CACHE PATH "Filename of static HTML file" PARENT_SCOPE)
mark_as_advanced(STATIC_HTML_PATH)
mark_as_advanced(STATIC_HTML_FILENAME)

add_library(pico_ws_server
  src/client_connection.cpp
  src/http_handler.cpp
  src/web_socket_frame_builder.cpp
  src/web_socket_handler.cpp
  src/web_socket_message_builder.cpp
  src/web_socket_server.cpp
  src/web_socket_server_internal.cpp
)

if(NOT STATIC_HTML_PATH)
    message(FATAL_ERROR "STATIC_HTML_PATH must be set")
endif()
if(NOT STATIC_HTML_FILENAME)
    message(FATAL_ERROR "STATIC_HTML_FILENAME must be set")
endif()

target_include_directories(pico_ws_server PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include )
target_include_directories(pico_ws_server PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src )
target_include_directories(pico_ws_server PRIVATE ${PROJECT_BINARY_DIR})

target_compile_definitions(pico_ws_server PUBLIC DEBUG_PRINT=0)

target_link_libraries(pico_ws_server
  pico_stdlib
  pico_cyw43_driver
  pico_lwip_nosys
  pico_mbedtls
  lwipopts_provider
)

add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/static_html_hex.h
    DEPENDS ${STATIC_HTML_PATH}
    COMMAND gzip --best -c ${STATIC_HTML_PATH}/${STATIC_HTML_FILENAME} > ${PROJECT_BINARY_DIR}/static.html.gz
    COMMAND ${CMAKE_COMMAND} -E echo "\\#ifndef STATIC_HTML_HEX" >> ${PROJECT_BINARY_DIR}/static_html_hex.h
    COMMAND ${CMAKE_COMMAND} -E echo "\\#define STATIC_HTML_HEX" >> ${PROJECT_BINARY_DIR}/static_html_hex.h
    COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_BINARY_DIR} xxd -i static.html.gz >> ${PROJECT_BINARY_DIR}/static_html_hex.h
    COMMAND ${CMAKE_COMMAND} -E echo "\\#endif // STATIC_HTML_HEX" >> ${PROJECT_BINARY_DIR}/static_html_hex.h
)

add_custom_target(generate_static_html_hex ALL
    DEPENDS ${PROJECT_BINARY_DIR}/static_html_hex.h
)
add_dependencies(pico_ws_server generate_static_html_hex)
