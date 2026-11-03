// Native backend query: no guest EXE, window, swapchain, or game code.
#include <d3d9.h>
#include <cstdio>
#include <exception>

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("DXVK native Android adapter query (no surface/game execution)");
  IDirect3D9* d3d = nullptr;
  try {
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::puts("FAIL Direct3DCreate9 returned null"); return 1; }
    const UINT count = d3d->GetAdapterCount();
    std::printf("adapter_count=%u\n", count);
    if (!count) { d3d->Release(); return 2; }
    for (UINT index = 0; index < count; ++index) {
      D3DADAPTER_IDENTIFIER9 id {};
      HRESULT hr = d3d->GetAdapterIdentifier(index, 0, &id);
      std::printf("adapter[%u] identity_hr=0x%08x description=%s driver=%s vendor=0x%x device=0x%x\n",
                  index, unsigned(hr), id.Description, id.Driver, unsigned(id.VendorId), unsigned(id.DeviceId));
      if (FAILED(hr)) { d3d->Release(); return 3; }
      D3DCAPS9 caps {};
      hr = d3d->GetDeviceCaps(index, D3DDEVTYPE_HAL, &caps);
      std::printf("adapter[%u] caps_hr=0x%08x vs=0x%x ps=0x%x max_texture=%ux%u max_streams=%u\n",
                  index, unsigned(hr), unsigned(caps.VertexShaderVersion), unsigned(caps.PixelShaderVersion),
                  unsigned(caps.MaxTextureWidth), unsigned(caps.MaxTextureHeight), unsigned(caps.MaxStreams));
      if (FAILED(hr)) { d3d->Release(); return 4; }
      const D3DFORMAT formats[] = {D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5};
      const char* names[] = {"A8R8G8B8", "X8R8G8B8", "DXT1", "DXT3", "DXT5"};
      for (unsigned f = 0; f < 5; ++f) {
        hr = d3d->CheckDeviceFormat(index, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, formats[f]);
        std::printf("adapter[%u] texture_%s_hr=0x%08x supported=%d\n", index, names[f], unsigned(hr), SUCCEEDED(hr));
      }
    }
    d3d->Release();
    std::puts("PASS native adapter/caps query; no VkDevice/swapchain/gameplay tested");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL native DXVK exception: %s\n", error.what());
  } catch (...) {
    std::fputs("FAIL unknown native DXVK exception\n", stderr);
  }
  if (d3d) d3d->Release();
  return 5;
}
