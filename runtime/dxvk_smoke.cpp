/* Real D3D9 -> DXVK -> Vulkan draw/readback/present, independent of game data. */
#include <d3d9.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <time.h>
#include "dxvk_smoke.h"

extern "C" void *dxvkAndroidGetWindowHandle(void);

static bool checked(HRESULT result, const char *operation) {
    if (SUCCEEDED(result)) return true;
    std::fprintf(stderr,"[dxvk-test] %s failed: 0x%08X\n",operation,unsigned(result));
    return false;
}

int tf_dxvk_smoke(uint32_t width, uint32_t height) {
    if (width < 16 || height < 16 || width > 8192 || height > 8192) return 10;
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::fprintf(stderr,"[dxvk-test] Direct3DCreate9 failed\n"); return 11; }
    D3DADAPTER_IDENTIFIER9 adapter{};
    if (SUCCEEDED(d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT,0,&adapter)))
        std::fprintf(stderr,"[dxvk-test] adapter: %s\n",adapter.Description);
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = width; pp.BackBufferHeight = height;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE;
    pp.hDeviceWindow = static_cast<HWND>(dxvkAndroidGetWindowHandle());
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    IDirect3DDevice9 *device = nullptr;
    HRESULT result = d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,pp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,&pp,&device);
    if (!checked(result,"CreateDevice")) { d3d->Release(); return 12; }
    struct Vertex { float x,y,z,rhw; uint32_t color; };
    static_assert(sizeof(Vertex)==20,"D3D9 XYZRHW + DIFFUSE layout");
    const Vertex vertices[] = {
        {width*.50f,height*.18f,.5f,1.f,0xff25c47a},
        {width*.15f,height*.82f,.5f,1.f,0xff25c47a},
        {width*.85f,height*.82f,.5f,1.f,0xff25c47a}
    };
    int status = 0;
    auto setup = [&]() {
        return checked(device->SetRenderState(D3DRS_LIGHTING,FALSE),"SetRenderState lighting") &&
            checked(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"SetRenderState cull") &&
            checked(device->SetRenderState(D3DRS_ZENABLE,FALSE),"SetRenderState depth") &&
            checked(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE),"SetFVF");
    };
    if (!setup()) status=13;
    for (unsigned frame=0; !status && frame<180; ++frame) {
        if (!checked(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff101b2b,1.f,0),"Clear") ||
            !checked(device->BeginScene(),"BeginScene")) { status=14; break; }
        bool draw = checked(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,sizeof(Vertex)),"DrawPrimitiveUP");
        bool end = checked(device->EndScene(),"EndScene");
        if (!draw || !end) { status=15; break; }
        if (!frame) {
            IDirect3DSurface9 *render=nullptr,*readback=nullptr;
            bool read = checked(device->GetRenderTarget(0,&render),"GetRenderTarget") &&
                checked(device->CreateOffscreenPlainSurface(width,height,D3DFMT_X8R8G8B8,
                    D3DPOOL_SYSTEMMEM,&readback,nullptr),"CreateOffscreenPlainSurface") &&
                checked(device->GetRenderTargetData(render,readback),"GetRenderTargetData");
            D3DLOCKED_RECT lock{};
            if (read && checked(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY),"LockRect")) {
                uint32_t center=0,corner=0;
                auto bytes=static_cast<const unsigned char*>(lock.pBits);
                std::memcpy(&center,bytes + size_t(height/2)*size_t(lock.Pitch) + size_t(width/2)*4,4);
                std::memcpy(&corner,bytes + size_t(2)*size_t(lock.Pitch) + size_t(2)*4,4);
                read = checked(readback->UnlockRect(),"UnlockRect") &&
                    (center & 0xffffff)==0x25c47a && (corner & 0xffffff)==0x101b2b;
                std::fprintf(stderr,"[dxvk-test] pixel readback center=%08X corner=%08X match=%s\n",
                    center,corner,read ? "yes":"no");
            } else read=false;
            if (readback) readback->Release();
            if (render) render->Release();
            if (!read) { status=16; break; }
        }
        if (!checked(device->Present(nullptr,nullptr,nullptr,nullptr),"Present")) { status=17; break; }
        timespec delay{0,16666667}; nanosleep(&delay,nullptr);
    }
    device->Release(); d3d->Release();
    if (!status) std::fprintf(stderr,"[dxvk-test] PASS: draw, exact pixel readback and 180 presents; no game executed\n");
    return status;
}
