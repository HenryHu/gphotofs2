cmake_minimum_required(VERSION 2.8)

project(gphotofs2)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFUSE_USE_VERSION=26")

find_package(PkgConfig REQUIRED)

pkg_check_modules(FUSE REQUIRED fuse)
include_directories(${FUSE_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_CFLAGS_OTHER}")

pkg_check_modules(GPHOTO2 REQUIRED libgphoto2)
include_directories(${GPHOTO2_INCLUDE_DIRS})

link_directories(${FUSE_LIBDIR} ${GPHOTO2_LIBDIR})
add_executable(gphotofs2
    gphotofs2.cpp
    dir.cpp
    utils.cpp
    context.cpp)
target_link_libraries(gphotofs2 ${FUSE_LIBRARIES} ${GPHOTO2_LIBRARIES})
set_property(TARGET gphotofs2 PROPERTY CXX_STANDARD 11)
