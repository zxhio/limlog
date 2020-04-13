cmake_minimum_required(VERSION 2.9)
project(limlog C CXX)

# set(CMAKE_VERBOSE_MAKEFILE ON)
option(LIMLOG_CLANG "clang++" OFF)

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
	set(CMAKE_CXX_COMPILER "g++")
	set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wall -Werror")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__REL_FILE__='\"$(subst ${PROJECT_SOURCE_DIR}/,,$(abspath $<))\"'")

if (LIMLOG_CLANG)
	set(CMAKE_CXX_COMPILER "clang++")
endif()

add_subdirectory(lim)
add_subdirectory(tests)