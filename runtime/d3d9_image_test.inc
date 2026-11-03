/* Synthetic COM regression: no game data, GPU, window, or device screenshot.
 * Included here to exercise the production registry and encoder together. */
namespace {
struct ImageTestSurface final:IDirect3DSurface9 {
    ULONG refs=1;bool locked=false;unsigned locks=0,unlocks=0;
    unsigned char pixels[32]={0,0,255,0,0,255,0,0,255,0,0,0,0,0,0,0,
        255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0};
    HRESULT QueryInterface(REFIID iid,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IDirect3DSurface9))return E_NOINTERFACE;
        *out=this;AddRef();return S_OK;
    }
    ULONG AddRef()override{return ++refs;}
    ULONG Release()override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT GetDevice(IDirect3DDevice9 **out)override{if(out)*out=nullptr;return E_NOTIMPL;}
    HRESULT SetPrivateData(REFGUID,const void*,DWORD,DWORD)override{return E_NOTIMPL;}
    HRESULT GetPrivateData(REFGUID,void*,DWORD*)override{return E_NOTIMPL;}
    HRESULT FreePrivateData(REFGUID)override{return E_NOTIMPL;}
    DWORD SetPriority(DWORD)override{return 0;}
    DWORD GetPriority()override{return 0;}
    void PreLoad()override{}
    D3DRESOURCETYPE GetType()override{return D3DRTYPE_SURFACE;}
    HRESULT GetContainer(REFIID,void **out)override{if(out)*out=nullptr;return E_NOINTERFACE;}
    HRESULT GetDesc(D3DSURFACE_DESC *out)override{
        if(!out)return D3DERR_INVALIDCALL;
        *out={D3DFMT_A8R8G8B8,D3DRTYPE_SURFACE,0,D3DPOOL_SYSTEMMEM,D3DMULTISAMPLE_NONE,0,3,2};return S_OK;
    }
    HRESULT LockRect(D3DLOCKED_RECT *out,const RECT *rect,DWORD flags)override{
        if(!out||rect||locked||flags!=D3DLOCK_READONLY)return D3DERR_INVALIDCALL;
        out->Pitch=16;out->pBits=pixels;locked=true;++locks;return S_OK;
    }
    HRESULT UnlockRect()override{if(!locked)return D3DERR_INVALIDCALL;locked=false;++unlocks;return S_OK;}
    HRESULT GetDC(HDC*)override{return E_NOTIMPL;}
    HRESULT ReleaseDC(HDC)override{return E_NOTIMPL;}
};
}
extern "C" int tf_d3d9_image_selftest(){
    uint32_t memory=allocation(64);int result=0;
    auto run=[&]()->int{
        auto surface=new ImageTestSurface;uint32_t source=wrap(surface,Surface),count=0;
        uint32_t args[]={memory,0,source,0,0};
        if(tf_d3dx9_dispatch(8,args,&count)!=S_OK||count!=5||!read(memory))return 140;
        auto inspect=[&](uint32_t expected_width,uint32_t expected_height,uint32_t first_pixel)->bool{
            uint32_t call[]={read(memory)},n=0;
            uint32_t size=tf_d3d9_dispatch(Buffer,4,call,&n);if(n!=1)return false;
            uint32_t address=tf_d3d9_dispatch(Buffer,3,call,&n);if(n!=1||!address)return false;
            TF_BMPInfo image{};auto bmp=guest<unsigned char>(address,size);
            if(!tf_bmp_parse(bmp,size,&image)||image.width!=expected_width||image.height!=expected_height||size!=tf_bmp_size(expected_width,expected_height))return false;
            unsigned char pixels[24]{};
            if(!tf_bmp_decode(bmp,size,pixels,expected_width*4,sizeof(pixels))||tf_bmp_u32(pixels)!=first_pixel)return false;
            return tf_d3d9_dispatch(Buffer,2,call,&n)==0&&n==1;
        };
        if(!inspect(3,2,0xffff0000u)||surface->locked||surface->locks!=1||surface->unlocks!=1)return 141;
        *guest<RECT>(memory+16)={1,0,3,1};args[4]=memory+16;
        if(tf_d3dx9_dispatch(8,args,&count)!=S_OK||!inspect(2,1,0xff00ff00u))return 142;
        guest<RECT>(memory+16)->right=4;
        if(tf_d3dx9_dispatch(8,args,&count)!=uint32_t(D3DERR_INVALIDCALL)||read(memory)||surface->locked||surface->locks!=2||surface->unlocks!=2)return 143;
        args[4]=0;args[1]=3;
        if(tf_d3dx9_dispatch(8,args,&count)!=uint32_t(D3DERR_INVALIDCALL)||read(memory))return 144;
        args[0]=0;if(tf_d3dx9_dispatch(8,args,&count)!=uint32_t(E_POINTER))return 145;
        uint32_t call[]={source};if(tf_d3d9_dispatch(Surface,2,call,&count)!=0)return 146;
        std::fprintf(stderr,"[d3dx-image-test] PASS: guest32 COM BMP save/buffer size/pointer/release, exact colors, padded rows, crop, invalid rectangle/format/output, balanced surface locks; no GPU or game executed\n");
        return 0;
    };
    try{result=run();}catch(const std::exception& error){std::fprintf(stderr,"[d3dx-image-test] %s\n",error.what());result=147;}
    release_guest(memory);return result;
}
