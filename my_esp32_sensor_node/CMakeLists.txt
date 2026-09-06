cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "hardware"
    "middle"
    "application"
)

set(PROJECT_VER "1.0.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sensor_node)
