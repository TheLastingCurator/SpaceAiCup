
cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)
################### Variables. ####################
# Change if you want modify path or other values. #
###################################################


set(CMAKE_MACOSX_BUNDLE 1)
# Define Release by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}.")

set(PROJECT_NAME subleqasm)
# Output Variables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# Folders files
set(DATA_DIR .)
set(CPP_DIR_2 .)
set(HEADER_DIR_2 .)

file(GLOB_RECURSE RES_SOURCES "${DATA_DIR}/data/*")

SET(CMAKE_CXX_COMPILER             "/usr/bin/clang++")
set(CMAKE_CXX_STANDARD 14)
set(THREADS_PREFER_PTHREAD_FLAG ON)
############## Define Project. ###############
# ---- This the main options of project ---- #
##############################################

project(${PROJECT_NAME} CXX)
ENABLE_LANGUAGE(C)


include_directories(${CMAKE_SOURCE_DIR}/..)

################# Flags ################
# Defines Flags for Windows and Linux. #
########################################

message(STATUS "CompilerId: ${CMAKE_CXX_COMPILER_ID}.")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O3")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang++" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

################ Files ################
#   --   Add files to project.   --   #
#######################################

file(GLOB SRC_FILES
    ${CPP_DIR_2}/*.cpp
    ${CPP_DIR_2}/*.c
    ${HEADER_DIR_2}/*.h
    ${HEADER_DIR_2}/*.hpp
)

# Add executable to build.
add_executable(${PROJECT_NAME} MACOSX_BUNDLE
   ${SRC_FILES}
   ${RES_SOURCES}
)

target_link_libraries(
  ${PROJECT_NAME}
)
