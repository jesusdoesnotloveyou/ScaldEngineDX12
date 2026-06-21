# general info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
# project info
set(PROJECT_NAME ScaldEngineDX12) # solution name
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX C)

# common options
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for executables (.EXE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for static lib (.LIB)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for shared lib (.DLL)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SUPPRESS_REGENERATION TRUE)

# rest of the actual Engine source code
add_subdirectory(Engine)

include(${CMAKE_SOURCE_DIR}/Automation/CMAKE/CMakeHelpers.cmake)
# ======================================================================
# debug info
print_system_info()