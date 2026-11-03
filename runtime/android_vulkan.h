#pragma once
// Called once per game process, before DXVK creates its Vulkan instance.
void tf_android_configure_vulkan_driver(const char* game_directory,const char* mode,const char* selected_path);
extern "C" int tf_android_driver_probe(const char* mode,const char* selected_path);
