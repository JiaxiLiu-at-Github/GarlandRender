cmake_minimum_required(VERSION 2.8)

# include the project setting file
include($ENV{DEVKIT_LOCATION}/cmake/pluginEntry.cmake)

# specify project name
set(PROJECT_NAME Garland)

# set SOURCE_FILES
set(SOURCE_FILES
   GarlandRender.cpp
   GarlandPlugin.cpp
   GarlandRender.h
   DxManager.h
   DxManager.cpp
)

# set linking libraries
set(LIBRARIES
     OpenMaya
     OpenMayaRender
     Foundation
)

if(WIN32)
	set(dx11_libs d3d11 d3dx11)
	find_directX("${dx11_libs}")
endif()

# Build plugin
build_plugin()

