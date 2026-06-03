set(PROJECT_NAME ScaldCommonLib)

# current targets' sources
set(SCALD_COMMON_INCLUDES
	KeyboardDevice.h
	MousePad.h
	ScaldCommonDefines.h
	ScaldMath.h
	ScaldTimer.h
)
set(SCALD_COMMON_SOURCES
	KeyboardDevice.cpp
	MousePad.cpp
	ScaldMath.cpp
	ScaldTimer.cpp
)

add_library(${PROJECT_NAME} STATIC ${SCALD_COMMON_INCLUDES} ${SCALD_COMMON_SOURCES})

# pch
target_precompile_headers(${PROJECT_NAME} PUBLIC # PUBLIC to expand using of this pch for ALL users of ScaldCommonLib
	${CMAKE_SOURCE_DIR}/Engine/Source/stdafx.h
)

# link libraries to ScaldCommon
target_link_libraries(${PROJECT_NAME}
	# TODO: I think it's better to separate DirectXTK12 to different library (to avoid linking to GameFramework), but for now it is here
	PUBLIC dxgi.lib dxguid.lib d3d12.lib d3dcompiler.lib DirectXTex DirectXTK12 # PUBLIC for all users of ScaldCommonLib
)

# includes
target_include_directories(${PROJECT_NAME} PUBLIC
	${directxtk_SOURCE_DIR}/Src # for d3dx12.h
)

if (MSVC)
	target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE) # CHAR WCHAR windows stuff, UNICODE forces using of wide strings and so on
endif()

# =============================================================
# debug info
message("================== ${PROJECT_NAME} ==================")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("=======================================================")