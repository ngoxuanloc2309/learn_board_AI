# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Layer components of LoganSDK. Each directory is one ESP-IDF component and
# the component name is the directory name. This must come before project.cmake.
# libs holds third party sources (FatFs now). cJSON is in that folder but is not built yet.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/app
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/board
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/components/modules
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/components/peripherals
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/libs
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/platform/esp32s3
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/services
    ${CMAKE_CURRENT_LIST_DIR}/LoganSDK/utils
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(board_hmi_ai)