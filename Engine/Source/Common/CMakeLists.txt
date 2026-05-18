set(PROJECT_NAME ScaldCommonLib)

# current targets' sources
set(SCALD_COMMON_INCLUDES
	DXHelper.h
	ScaldCommonDefines.h
	ScaldTimer.h
	Shapes.h
)
set(SCALD_COMMON_SOURCES
	ScaldTimer.cpp
	Shapes.cpp
)
set(MATH_INCLUDES ScaldMath.h)
set(MATH_SOURCES ScaldMath.cpp)

add_library(MathLib STATIC ${MATH_INCLUDES} ${MATH_SOURCES})
add_library(${PROJECT_NAME} STATIC ${SCALD_COMMON_INCLUDES} ${SCALD_COMMON_SOURCES})

# pch
target_precompile_headers(${PROJECT_NAME} PUBLIC # PUBLIC to expand using of this pch for ALL users of ScaldCommonLib
	${CMAKE_SOURCE_DIR}/Engine/Source/stdafx.h
)

# link libraries to ScaldCommon
target_link_libraries(${PROJECT_NAME}
	# TODO: I think it's better to separate DirectXTK12 to different library (to avoid linking to GameFramework), but for now it is here
	PUBLIC dxgi.lib dxguid.lib d3d12.lib d3dcompiler.lib DirectXTK12 # PUBLIC for all users of ScaldCommonLib
)

# includes
target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_SOURCE_DIR}/Engine/Source
	${directxtk_SOURCE_DIR}/Src # for d3dx12.h
)

message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")