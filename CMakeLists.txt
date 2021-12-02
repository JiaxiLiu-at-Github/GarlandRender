cmake_minimum_required(VERSION 3.0)


set(PROJECT_NAME Garland)
project(${PROJECT_NAME})

# include the project setting file
include($ENV{DEVKIT_LOCATION}/cmake/pluginEntry.cmake)

# set SOURCE_FILES
set(SOURCE_FILES
   GarlandRender.cpp
   GarlandPlugin.cpp
   GarlandRender.h
   DxManager.h
   DxManager.cpp
   util.h
   util.cpp
)

# set linking libraries
set(LIBRARIES
     OpenMaya
     OpenMayaUI
     OpenMayaRender
     Foundation
)

if(WIN32)
	set(dx11_libs d3d11)
	find_directX("${dx11_libs}")
endif()

# Build plugin
build_plugin()

add_subdirectory(shaders)
add_dependencies(${PROJECT_NAME} ShaderCompile)