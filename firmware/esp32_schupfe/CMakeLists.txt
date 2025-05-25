# The following four lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Point to shared components (relative to this firmware project)
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/../common_components/custom_common
    ${CMAKE_SOURCE_DIR}/../common_components/pzem004tv3
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pv_power-meter_schupfe)
