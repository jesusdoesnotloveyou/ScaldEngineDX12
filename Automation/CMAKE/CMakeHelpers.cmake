function(print_system_info) # could be macro(print_system_info)
    message("====================== System info ======================")

    if(${WIN32})
    message("Running on Windows")
    elseif(${LINUX})
    message("Running on Linux")
    endif()

    if(${MSVC})
    message("MSVC version: ${MSVC_VERSION}")
    message("MSVC toolset version: ${MSVC_TOOLSET_VERSION}")
    endif()

    message("Using ${CMAKE_CXX_COMPILER_ID} compiler")
    message("Compiler flags: ${CMAKE_CXX_FLAGS}")
    message("Compiler debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
    message("Compiler release flags: ${CMAKE_CXX_FLAGS_RELEASE}")

    message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
    message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
    message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
    message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    message("Configuration types: ${CMAKE_CONFIGURATION_TYPES}")
    message("=========================================================")
endfunction() # endmacro()