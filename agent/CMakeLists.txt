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
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED IMPORTED_TARGET libpcre2-8)

set(prompt_src
  src/nabtoshell_prompt.c
  src/nabtoshell_pattern_config.c
  src/nabtoshell_terminal_state.c
  src/nabtoshell_prompt_rules.c
  src/nabtoshell_prompt_lifecycle.c
  src/nabtoshell_prompt_detector.c
  src/nabtoshell_prompt_protocol.c
)

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
  src/nabtoshell_control_stream.c
  src/nabtoshell_stream.c
  src/nabtoshell_session.c
  src/nabtoshell_tmux.c
  src/nabtoshell_banner.c
  ${prompt_src}
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
  PkgConfig::PCRE2
)

if(NOT APPLE)
  target_link_libraries(nabtoshell-agent util)
endif()

install(TARGETS nabtoshell-agent RUNTIME DESTINATION bin)

pkg_check_modules(CHECK IMPORTED_TARGET check)
if(CHECK_FOUND)
  enable_testing()

  add_library(nabtoshell_prompt STATIC ${prompt_src})
  target_include_directories(nabtoshell_prompt PUBLIC src)
  target_link_libraries(nabtoshell_prompt PUBLIC
    cjson PkgConfig::PCRE2 TinyCBOR::tinycbor Threads::Threads)

  add_executable(test_prompt_config tests/test_prompt_config.c)
  target_link_libraries(test_prompt_config nabtoshell_prompt PkgConfig::CHECK)
  add_test(NAME prompt_config COMMAND test_prompt_config)

  add_executable(test_terminal_state tests/test_terminal_state.c)
  target_link_libraries(test_terminal_state nabtoshell_prompt PkgConfig::CHECK)
  add_test(NAME terminal_state COMMAND test_terminal_state)

  add_executable(test_prompt_rules tests/test_prompt_rules.c)
  target_link_libraries(test_prompt_rules nabtoshell_prompt PkgConfig::CHECK)
  add_test(NAME prompt_rules COMMAND test_prompt_rules)

  add_executable(test_prompt_lifecycle tests/test_prompt_lifecycle.c)
  target_link_libraries(test_prompt_lifecycle nabtoshell_prompt PkgConfig::CHECK)
  add_test(NAME prompt_lifecycle COMMAND test_prompt_lifecycle)

  add_executable(test_prompt_protocol tests/test_prompt_protocol.c)
  target_link_libraries(test_prompt_protocol nabtoshell_prompt PkgConfig::CHECK)
  add_test(NAME prompt_protocol COMMAND test_prompt_protocol)

  add_executable(test_prompt_detector_replay tests/test_prompt_detector_replay.c)
  target_link_libraries(test_prompt_detector_replay nabtoshell_prompt PkgConfig::CHECK)
  target_compile_definitions(test_prompt_detector_replay PRIVATE
    TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  add_test(NAME prompt_detector_replay COMMAND test_prompt_detector_replay)
endif()
