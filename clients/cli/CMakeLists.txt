cmake_minimum_required(VERSION 3.14)
project(NabtoShellCLI C)

find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
  tinycbor
  GIT_REPOSITORY https://github.com/intel/tinycbor.git
  GIT_TAG        v0.6.0
)
FetchContent_Populate(tinycbor)

set(TINYCBOR_SRC
  ${tinycbor_SOURCE_DIR}/src/cborencoder.c
  ${tinycbor_SOURCE_DIR}/src/cborparser.c
  ${tinycbor_SOURCE_DIR}/src/cborparser_dup_string.c
  ${tinycbor_SOURCE_DIR}/src/cborvalidation.c
)
add_library(tinycbor_lib STATIC ${TINYCBOR_SRC})
target_include_directories(tinycbor_lib PUBLIC ${tinycbor_SOURCE_DIR}/src)

set(src
  src/main.c
  src/nabtoshell_pair.c
  src/nabtoshell_attach.c
  src/nabtoshell_sessions.c
  src/nabtoshell_devices.c
  src/nabtoshell_rename.c
  src/nabtoshell_client_config.c
  src/nabtoshell_client_coap.c
  src/nabtoshell_client_util.c
  src/3rdparty/cjson/cJSON.c
)

add_executable(nabtoshell ${src})

target_include_directories(nabtoshell PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/nabto_client_sdk_library/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(APPLE)
  set(NABTO_CLIENT_SDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nabto_client_sdk_library/lib/macos-universal)
elseif(UNIX)
  set(NABTO_CLIENT_SDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nabto_client_sdk_library/lib/linux-x86_64)
endif()

target_link_directories(nabtoshell PRIVATE ${NABTO_CLIENT_SDK_LIB_DIR})

target_link_libraries(nabtoshell
  nabto_client
  tinycbor_lib
  Threads::Threads
)

set_target_properties(nabtoshell PROPERTIES
  BUILD_RPATH "${NABTO_CLIENT_SDK_LIB_DIR}"
  INSTALL_RPATH "${NABTO_CLIENT_SDK_LIB_DIR}"
)

install(TARGETS nabtoshell RUNTIME DESTINATION bin)
