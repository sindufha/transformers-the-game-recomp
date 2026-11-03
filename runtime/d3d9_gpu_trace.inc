/* Opt-in GPU fault isolation. Absent the private gpu-debug-draws marker,
 * no query, dump, timing change or draw interception occurs. */
static bool gpu_draw_trace_enabled() {
    static const bool enabled=access("gpu-debug-draws",F_OK)==0;
    return enabled;
}
template<class Shader>static void gpu_trace_shader(Shader* shader,const char* stage) {
    if(!shader)return;
    static std::map<Shader*,bool> captured;if(captured.count(shader))return;
    UINT bytes=0;if(FAILED(shader->GetFunction(nullptr,&bytes))||!bytes||bytes>1024*1024)return;
    std::vector<unsigned char> code(bytes);if(FAILED(shader->GetFunction(code.data(),&bytes)))return;
    char path[128];std::snprintf(path,sizeof(path),"gpu-debug-shaders/%s-%p.bin",stage,static_cast<void*>(shader));
    if(FILE* file=std::fopen(path,"wb")){std::fwrite(code.data(),1,bytes,file);std::fclose(file);captured[shader]=true;}
}
static HRESULT gpu_trace_draw(IDirect3DDevice9* device,uint32_t method,const uint32_t* a) {
    static uint64_t sequence=0;
    const auto id=++sequence;
    IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    device->GetVertexShader(&vs);device->GetPixelShader(&ps);
    std::fprintf(stderr,"[gpu-trace] draw %llu slot=%u VS=%p PS=%p args=%x,%x,%x\n",
        static_cast<unsigned long long>(id),method,static_cast<void*>(vs),static_cast<void*>(ps),a[1],a[2],a[3]);
    gpu_trace_shader(vs,"vs");gpu_trace_shader(ps,"ps");
    if(method==81){
        IDirect3DSurface9* target=nullptr;
        if(SUCCEEDED(device->GetRenderTarget(0,&target))&&target){D3DSURFACE_DESC desc{};target->GetDesc(&desc);
            std::fprintf(stderr,"[gpu-trace] RT0=%p %ux%u format=%u\n",static_cast<void*>(target),desc.Width,desc.Height,unsigned(desc.Format));target->Release();}
        IDirect3DBaseTexture9* texture=nullptr;
        if(SUCCEEDED(device->GetTexture(0,&texture))&&texture){
            std::fprintf(stderr,"[gpu-trace] texture0=%p type=%u\n",static_cast<void*>(texture),unsigned(texture->GetType()));
            if(texture->GetType()==D3DRTYPE_TEXTURE){auto t=static_cast<IDirect3DTexture9*>(texture);D3DSURFACE_DESC desc{};t->GetLevelDesc(0,&desc);
                IDirect3DSurface9* sampled=nullptr;t->GetSurfaceLevel(0,&sampled);
                std::fprintf(stderr,"[gpu-trace] texture0 surface=%p %ux%u format=%u\n",static_cast<void*>(sampled),desc.Width,desc.Height,unsigned(desc.Format));if(sampled)sampled->Release();}
            texture->Release();}
    }
    if(vs)vs->Release();if(ps)ps->Release();
    HRESULT result;
    switch(method){
    case 81:result=device->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(a[1]),a[2],a[3]);break;
    case 82:result=device->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(a[1]),static_cast<INT>(a[2]),a[3],a[4],a[5],a[6]);break;
    case 83:result=device->DrawPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(a[1]),a[2],pointer(a[3]),a[4]);break;
    default:result=device->DrawIndexedPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(a[1]),a[2],a[3],a[4],pointer(a[5]),static_cast<D3DFORMAT>(a[6]),pointer(a[7]),a[8]);break;
    }
    if(FAILED(result))return result;
    IDirect3DQuery9* query=nullptr;
    auto status=device->CreateQuery(D3DQUERYTYPE_EVENT,&query);
    if(FAILED(status))unsupported("GPU trace event query creation failed");
    status=query->Issue(D3DISSUE_END);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    if(SUCCEEDED(status))do {
        status=query->GetData(nullptr,0,D3DGETDATA_FLUSH);
        if(status!=S_FALSE)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }while(std::chrono::steady_clock::now()<deadline);
    query->Release();
    std::fprintf(stderr,"[gpu-trace] completed draw %llu status=%08x\n",static_cast<unsigned long long>(id),uint32_t(status));
    if(status!=S_OK)unsupported("GPU trace completion failed at draw "+std::to_string(id));
    return result;
}
