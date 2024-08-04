cmake_minimum_required(VERSION 2.8.12)
project(audacious-rpc)

# General options
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Finding Audacious
include(FindPkgConfig)
PKG_SEARCH_MODULE(AUDACIOUS REQUIRED audacious>=4.0)
add_definitions(${AUDACIOUS_CFLAGS})

# Building discord-rpc
include(FetchContent)

option(BUILD_EXAMPLES "Build examples for discord-rpc" OFF)
FetchContent_Declare(
  discord-rpc
  URL https://github.com/Kenny-Hui/discord-rpc/archive/refs/heads/master.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_MakeAvailable(discord-rpc)

find_file(RAPIDJSONTEST NAMES rapidjson rapidjson-1.1.0 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty CMAKE_FIND_ROOT_PATH_BOTH)
if (NOT RAPIDJSONTEST)
    message("no rapidjson, download")
    set(RJ_TAR_FILE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/v1.1.0.tar.gz)
    file(DOWNLOAD https://github.com/miloyip/rapidjson/archive/v1.1.0.tar.gz ${RJ_TAR_FILE})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${RJ_TAR_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    )
    file(REMOVE ${RJ_TAR_FILE})
endif(NOT RAPIDJSONTEST)

find_file(RAPIDJSON NAMES rapidjson rapidjson-1.1.0 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty CMAKE_FIND_ROOT_PATH_BOTH)
add_library(rapidjson STATIC IMPORTED ${RAPIDJSON})

if(IS_DIRECTORY "${discord-rpc_SOURCE_DIR}")
  set_property(DIRECTORY ${discord-rpc_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

# Creating RPC plugin
add_library(audacious-plugin-rpc SHARED audacious-plugin-rpc.cc)
target_include_directories(audacious-plugin-rpc PRIVATE ${RAPIDJSON}/include)
target_link_libraries(audacious-plugin-rpc discord-rpc)
message("${discord-rpc_SOURCE_DIR}/include")
target_include_directories(audacious-plugin-rpc PRIVATE "${discord-rpc_SOURCE_DIR}/include")

# Installing RPC plugin
PKG_GET_VARIABLE(AUDACIOUS_PLUGIN_DIR audacious plugin_dir)
set(INSTALL_DIR "${AUDACIOUS_PLUGIN_DIR}/General")

install(TARGETS audacious-plugin-rpc LIBRARY DESTINATION ${INSTALL_DIR})
