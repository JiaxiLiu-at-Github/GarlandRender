# Garland-Render
GarlandRender is a C++ Maya plugin which serves as a template showing how to override THE Viewport Render using the DirectX pipeline. 

Referencing the code in devkit sample "devkitBase/devkit/plug-ins/viewDX11DeviceAccess"

## How to run the code?
- [Setting up the Maya devkit on Windows](https://help.autodesk.com/view/MAYAUL/2022/ENU/?guid=Maya_SDK_Setting_up_your_build_Windows_environment_64_bit_html)
- [Building with CMake](https://help.autodesk.com/view/MAYAUL/2022/ENU/?guid=Maya_SDK_SettingUpCMake_Building_with_cmake_html)

## How to use the plugin?
- Copy the *Garland.mll* to Maya plug-ins folder, in my case, it is 
```
C:/Users/<user-name>/Documents/maya/<version>/plug-ins/
```

- Load the plugin. In the viewport-Renderer, select GarlandViewport. This override sample draws the bounding box on top of the original viewport. 
<img src="usage.png" width="800">
