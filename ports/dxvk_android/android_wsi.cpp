// Project Android window-system adapter for DXVK Native 1.9.2b.
// The upstream DXVK source is separately licensed (see DXVK-LICENSE).
#include "android_wsi.h"
#include <android/native_window.h>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include "src/wsi/wsi_presenter.h"
#include "src/wsi/wsi_window.h"
#include "src/wsi/wsi_mode.h"
#include "src/dxvk/dxvk_platform_exts.h"

namespace {
std::mutex windowMutex;
ANativeWindow* nativeWindow = nullptr;
uintptr_t generation = 0x1000;
HWND nativeHandle = nullptr;
uint32_t screenWidth = 1280, screenHeight = 720;
uint32_t renderWidth = 0, renderHeight = 0;
HMONITOR primaryMonitor() { return reinterpret_cast<HMONITOR>(uintptr_t(1)); }
}

extern "C" void dxvkAndroidSetWindow(ANativeWindow* window) {
  std::lock_guard<std::mutex> lock(windowMutex);
  if (window == nativeWindow) return;
  if (window) {
    ANativeWindow_acquire(window);
    screenWidth = std::max(1, ANativeWindow_getWidth(window));
    screenHeight = std::max(1, ANativeWindow_getHeight(window));
  }
  if (nativeWindow) ANativeWindow_release(nativeWindow);
  nativeWindow = window;
  nativeHandle = window ? reinterpret_cast<HWND>(++generation) : nullptr;
}
extern "C" void* dxvkAndroidGetWindowHandle() {
  std::lock_guard<std::mutex> lock(windowMutex);
  return nativeHandle;
}
extern "C" void dxvkAndroidSetRenderSize(uint32_t width, uint32_t height) {
  std::lock_guard<std::mutex> lock(windowMutex);
  renderWidth = width;
  renderHeight = height;
  // One virtual desktop uses the selected game resolution. Android's surface
  // capabilities still determine the physical presentation image dimensions.
  if(width&&height){screenWidth=width;screenHeight=height;}
}

namespace dxvk {
DxvkPlatformExts DxvkPlatformExts::s_instance;
std::string_view DxvkPlatformExts::getName() { return "Android native surface"; }
DxvkNameSet DxvkPlatformExts::getInstanceExtensions() {
  DxvkNameSet names;
  names.add(VK_KHR_SURFACE_EXTENSION_NAME);
  names.add(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
  return names;
}
DxvkNameSet DxvkPlatformExts::getDeviceExtensions(uint32_t) { return {}; }
void DxvkPlatformExts::initInstanceExtensions() {}
void DxvkPlatformExts::initDeviceExtensions(const DxvkInstance*) {}
}

namespace dxvk::wsi {
VkResult createSurface(HWND window, const Rc<vk::InstanceFn>& instance, VkSurfaceKHR* surface) {
  std::lock_guard<std::mutex> lock(windowMutex);
  if (!surface || !nativeWindow || window != nativeHandle) return VK_ERROR_SURFACE_LOST_KHR;
  auto create = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
    vkGetInstanceProcAddr(instance->instance(), "vkCreateAndroidSurfaceKHR"));
  if (!create) return VK_ERROR_EXTENSION_NOT_PRESENT;
  VkAndroidSurfaceCreateInfoKHR info {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
  info.window = nativeWindow;
  return create(instance->instance(), &info, nullptr, surface);
}
HMONITOR getDefaultMonitor() { return primaryMonitor(); }
HMONITOR enumMonitors(uint32_t index) { return index == 0 ? primaryMonitor() : nullptr; }
bool getDisplayName(HMONITOR monitor, WCHAR (&name)[32]) {
  if (monitor != primaryMonitor()) return false;
  std::fill(std::begin(name), std::end(name), 0);
  const wchar_t display[] = L"\\\\.\\DISPLAY1";
  std::copy(std::begin(display), std::end(display), name);
  return true;
}
bool getDesktopCoordinates(HMONITOR monitor, RECT* rect) {
  if (monitor != primaryMonitor() || !rect) return false;
  std::lock_guard<std::mutex> lock(windowMutex);
  *rect = {0, 0, LONG(screenWidth), LONG(screenHeight)};
  return true;
}
bool getCurrentDisplayMode(HMONITOR monitor, WsiMode* mode) {
  if (monitor != primaryMonitor() || !mode) return false;
  std::lock_guard<std::mutex> lock(windowMutex);
  *mode = {screenWidth, screenHeight, {60, 1}, 32, false};
  return true;
}
bool getDesktopDisplayMode(HMONITOR monitor, WsiMode* mode) { return getCurrentDisplayMode(monitor, mode); }
bool getDisplayMode(HMONITOR monitor, uint32_t index, WsiMode* mode) {
  if (monitor != primaryMonitor() || !mode || index > 7) return false;
  std::lock_guard<std::mutex> lock(windowMutex);
  static constexpr uint32_t modes[][2] = {{640,480}, {800,600}, {1024,768}, {1280,720}, {1280,960}, {1280,1024}};
  uint32_t width = screenWidth, height = screenHeight;
  if (index < 6) { width = modes[index][0]; height = modes[index][1]; }
  if (index == 7) {
    if (!renderWidth || !renderHeight) return false;
    width = renderWidth; height = renderHeight;
  }
  *mode = {width, height, {60,1}, 32, false};
  return true;
}
void getWindowSize(HWND window, uint32_t* width, uint32_t* height) {
  std::lock_guard<std::mutex> lock(windowMutex);
  const bool valid = nativeWindow && window == nativeHandle;
  if (width) *width = valid ? screenWidth : 0;
  if (height) *height = valid ? screenHeight : 0;
}
void resizeWindow(HWND window, DxvkWindowState*, uint32_t width, uint32_t height) {
  std::lock_guard<std::mutex> lock(windowMutex);
  // Android owns the physical SurfaceView size. DXVK may request a smaller
  // guest window size (for example the game's internal render resolution),
  // but applying it to the native buffer makes the guest image occupy only a
  // corner of the full-screen surface. Keep presentation at the surface size;
  // the D3D9 presenter scales the source rectangle into that output.
  (void)width;
  (void)height;
}
bool isWindow(HWND window) {
  std::lock_guard<std::mutex> lock(windowMutex);
  return nativeWindow && window == nativeHandle;
}
HMONITOR getWindowMonitor(HWND window) { return isWindow(window) ? primaryMonitor() : nullptr; }
bool setWindowMode(HMONITOR monitor, HWND window, const WsiMode*, bool) { return monitor == primaryMonitor() && isWindow(window); }
bool enterFullscreenMode(HMONITOR monitor, HWND window, DxvkWindowState*, bool) { return monitor == primaryMonitor() && isWindow(window); }
bool leaveFullscreenMode(HWND window, DxvkWindowState*) { return isWindow(window); }
bool restoreDisplayMode(HMONITOR monitor) { return monitor == primaryMonitor(); }
}
