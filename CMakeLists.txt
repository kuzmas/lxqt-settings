cmake_minimum_required(VERSION 2.8.6)

project(liblxqt-settings)

if(NOT CMAKE_BUILD_TYPE)
    set( CMAKE_BUILD_TYPE Release )
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE MATCHES [Dd]ebug)
    message(STATUS "Debug build")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CMAKE_VERBOSE TRUE)
else()
    message(STATUS "Release build")
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

find_package(Qt4 REQUIRED)
set(CMAKE_AUTOMOC TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DCONF REQUIRED
  dconf
)

include_directories(
  ${QT_INCLUDES}
  ${DCONF_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(CMAKE_CXX_FLAGS "-DQT_NO_KEYWORDS -fno-exceptions")

set(liblxqt-settings_SRCS
  liblxqt-settings.cpp
  main.cpp
)

qt4_automoc(${liblxqt-settings_SRCS})

add_executable(liblxqt-settings
  ${liblxqt-settings_SRCS}
)

target_link_libraries(liblxqt-settings
  ${QT_QTCORE_LIBRARY}
  ${DCONF_LIBRARIES}
)

install(TARGETS
  liblxqt-settings
  RUNTIME DESTINATION bin
)
