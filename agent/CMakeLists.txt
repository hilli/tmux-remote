cmake_minimum_required(VERSION 3.14)
project(NabtoShellAgent C)

include(FetchContent)

FetchContent_Declare(
  nabto_embedded_sdk
  GIT_REPOSITORY https://github.com/nabto/nabto-embedded-sdk.git
  GIT_TAG        master
)

set(DEVICE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DEVICE_BUILD_APPS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nabto_embedded_sdk)

FetchContent_GetProperties(nabto_embedded_sdk SOURCE_DIR NABTO_SDK_SRC)
include(${NABTO_SDK_SRC}/nabto_primary_files.cmake)

find_package(Threads REQUIRED)

set(src
  ${ne_iam_src}
  src/main.c
  src/nabtoshell_device.c
  src/nabtoshell_iam.c
  src/nabtoshell_init.c
  src/nabtoshell_coap_handler.c
  src/nabtoshell_coap_resize.c
  src/nabtoshell_coap_sessions.c
  src/nabtoshell_coap_attach.c
  src/nabtoshell_coap_create.c
  src/nabtoshell_coap_status.c
  src/nabtoshell_stream.c
  src/nabtoshell_session.c
  src/nabtoshell_tmux.c
  src/nabtoshell_banner.c
)

add_executable(nabtoshell-agent ${src})

target_include_directories(nabtoshell-agent PRIVATE
  ${NABTO_SDK_SRC}/src
  ${NABTO_SDK_SRC}/include
  ${NABTO_SDK_SRC}
)

target_link_libraries(nabtoshell-agent
  nabto_device
  3rdparty_gopt
  cjson
  NabtoCommon::nn
  apps_common
  nm_fs_posix
  TinyCBOR::tinycbor
  Threads::Threads
)

if(NOT APPLE)
  target_link_libraries(nabtoshell-agent util)
endif()

install(TARGETS nabtoshell-agent RUNTIME DESTINATION bin)
