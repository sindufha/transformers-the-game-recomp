/* Private native DXVK adapter for the exact pinned backend. These public C++
 * methods drain submissions and invalidate presentation only: guest D3D9
 * devices, render targets, textures, shaders and all game state are preserved. */
#include "../work/dxvk-source/src/d3d9/d3d9_device.h"
#include "../work/dxvk-source/src/d3d9/d3d9_swapchain.h"

static void drain(IDirect3DDevice9 *device){
    auto native=static_cast<dxvk::D3D9DeviceEx*>(device);
    native->Flush();native->SynchronizeCsThread();native->GetDXVKDevice()->waitForIdle();
}
static void invalidate(IDirect3DSwapChain9 *swap){
    auto native=static_cast<dxvk::D3D9SwapChainEx*>(swap);
    native->Invalidate(native->GetPresentParams()->hDeviceWindow);
}
extern "C" void tf_android_suspend_device(void *pointer){
    auto device=static_cast<IDirect3DDevice9*>(pointer);drain(device);
    IDirect3DSwapChain9 *swap=nullptr;
    if(SUCCEEDED(device->GetSwapChain(0,&swap))&&swap){invalidate(swap);swap->Release();}
}
extern "C" void tf_android_suspend_swapchain(void *pointer){
    auto swap=static_cast<IDirect3DSwapChain9*>(pointer);IDirect3DDevice9 *device=nullptr;
    if(SUCCEEDED(swap->GetDevice(&device))&&device){drain(device);invalidate(swap);device->Release();}
}
