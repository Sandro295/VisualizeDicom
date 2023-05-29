cmake_minimum_required(VERSION 3.0)

project(ITK-VTK CXX)
# set(CMAKE_FIND_DEBUG_MODE 1)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(renderDICOM src/itkvtkdicom.cpp)
target_link_libraries(renderDICOM ${VTK_LIBRARIES} ${ITK_LIBRARIES} )
