/* Guest32 D3DX9 constant-table adapter. Method order follows the published
 * ID3DXConstantTable ABI (Wine include/d3dx9shader.h). No native pointers are
 * returned to guest code. Complex structs remain an explicit boundary. */
struct GuestConstants:GuestBuffer {
    struct Constant {uint32_t handle,description[12];};
    std::vector<Constant> constants;
    // Set*Constant copies synchronously into DXVK. Reuse conversion storage
    // across this table's serialized setters instead of malloc/free per call.
    std::vector<uint32_t> upload_scratch;
    uint32_t version,creator;
    uint32_t u32(uint32_t offset){if(uint64_t(offset)+4>size)unsupported("CTAB DWORD outside buffer");return read(data+offset);}
    uint32_t u16(uint32_t offset){if(uint64_t(offset)+2>size)unsupported("CTAB WORD outside buffer");uint16_t v;std::memcpy(&v,pointer(data+offset,2),2);return v;}
    uint32_t text(uint32_t offset){if(offset>=size)unsupported("CTAB string offset");for(uint32_t i=offset;i<size;++i)if(!*guest<char>(data+i))return data+offset;unsupported("unterminated CTAB string");}
    GuestConstants(const std::vector<unsigned char>& bytes):GuestBuffer(bytes.size()){
        std::memcpy(pointer(data,size),bytes.data(),size);
        if(size<28||u32(0)!=28)unsupported("invalid CTAB header");
        version=u32(8);creator=text(u32(4));uint32_t count=u32(12),offset=u32(16);
        if(count>4096||uint64_t(offset)+uint64_t(count)*20>size)unsupported("invalid CTAB constant count");
        for(uint32_t i=0;i<count;++i){uint32_t at=offset+i*20,type=u32(at+12);Constant c{};
            c.handle=allocation(4);write(c.handle,0x54464354);
            c.description[0]=text(u32(at));c.description[1]=u16(at+4);c.description[2]=u16(at+6);c.description[3]=u16(at+8);
            c.description[4]=u16(type);c.description[5]=u16(type+2);c.description[6]=u16(type+4);c.description[7]=u16(type+6);
            c.description[8]=u16(type+8);c.description[9]=u16(type+10);
            if(c.description[9]||!c.description[8]||!c.description[6]||!c.description[7]||c.description[6]>4||c.description[7]>4)unsupported("CTAB struct or invalid dimensions");
            c.description[10]=4*c.description[6]*c.description[7]*c.description[8];
            uint32_t defaults=u32(at+16);if(defaults){uint32_t n=c.description[3]*(c.description[1]==0?4:16);if(uint64_t(defaults)+n>size)unsupported("CTAB defaults outside buffer");c.description[11]=data+defaults;}
            constants.push_back(c);
        }
    }
    ~GuestConstants()override{for(auto& c:constants)release_guest(c.handle);}
    HRESULT QueryInterface(REFIID iid,void **out)override{if(iid==constant_iid){if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}return GuestBuffer::QueryInterface(iid,out);}
    Constant *find_constant(uint32_t handle){for(auto& c:constants)if(c.handle==handle)return &c;return nullptr;}
};

HRESULT constant_upload(GuestConstants *table,IDirect3DDevice9 *device,const GuestConstants::Constant& constant,const uint32_t *values,uint32_t registers){
    auto d=constant.description;if(registers>d[3])return D3DERR_INVALIDCALL;bool vertex=(table->version>>16)==0xfffe;
    if(d[1]==0)return vertex?device->SetVertexShaderConstantB(d[2],reinterpret_cast<const BOOL*>(values),registers):device->SetPixelShaderConstantB(d[2],reinterpret_cast<const BOOL*>(values),registers);
    if(d[1]==1)return vertex?device->SetVertexShaderConstantI(d[2],reinterpret_cast<const int*>(values),registers):device->SetPixelShaderConstantI(d[2],reinterpret_cast<const int*>(values),registers);
    if(d[1]==2)return vertex?device->SetVertexShaderConstantF(d[2],reinterpret_cast<const float*>(values),registers):device->SetPixelShaderConstantF(d[2],reinterpret_cast<const float*>(values),registers);
    return D3DERR_INVALIDCALL;
}
uint32_t constant_dispatch(GuestConstants *table,uint32_t method,const uint32_t *a,uint32_t *argc){
    static const uint32_t arguments[]={3,1,1,1,1,2,4,2,3,3,3,2,5,4,5,4,5,4,5,4,5,4,5,5,4,5,5};
    *argc=arguments[method];
    if(method==3)return table->data;if(method==4)return table->size;
    if(method==5){if(!a[1])return D3DERR_INVALIDCALL;write(a[1],table->creator);write(a[1]+4,table->version);write(a[1]+8,table->constants.size());return S_OK;}
    if(method==6){auto c=table->find_constant(a[1]);if(!c||!a[3])return D3DERR_INVALIDCALL;uint32_t n=read(a[3]);write(a[3],1);if(a[2]&&n)std::memcpy(pointer(a[2],48),c->description,48);return S_OK;}
    if(method==7){auto c=table->find_constant(a[1]);return c&&c->description[1]==3?c->description[2]:0xffffffffu;}
    if(method==8){if(a[1])unsupported("CTAB member enumeration");return a[2]<table->constants.size()?table->constants[a[2]].handle:0;}
    if(method==9){if(a[1])unsupported("CTAB member lookup");if(!a[2])return 0;
        uint32_t n=0;while(n<65536&&*guest<char>(a[2]+n))++n;if(n==65536)unsupported("CTAB constant name too long");
        std::string_view name(guest<char>(a[2],n),n);for(auto& c:table->constants)if(name==guest<char>(c.description[0]))return c.handle;return 0;}
    if(method==10){auto c=table->find_constant(a[1]);if(!c)return 0;if(c->description[8]==1)return a[2]==0?c->handle:0;unsupported("CTAB array-element handles");}
    auto device=native_object<IDirect3DDevice9>(a[1]);if(!device)return D3DERR_INVALIDCALL;
    if(method==11){for(auto& c:table->constants)if(c.description[11]){uint32_t bytes=c.description[3]*(c.description[1]==0?4:16);auto result=constant_upload(table,device,c,guest<uint32_t>(c.description[11],bytes/4),c.description[3]);if(FAILED(result))return result;}return S_OK;}
    auto c=table->find_constant(a[2]);if(!c)return D3DERR_INVALIDCALL;auto d=c->description;
    if(d[1]>2||d[4]>3) return D3DERR_INVALIDCALL;
    // Conversion follows the declared register set; padding is deterministic.
    uint32_t elements=1,input_type=3,source=0,available=0,stride=0;bool matrix=method>=21,transpose=method>=24,indirect=method==23||method==26;
    uint32_t scalar=a[3];bool scalar_arg=method==13||method==15||method==17;
    if(method==12){source=a[3];available=a[4]/4;input_type=d[5];elements=std::min(d[8],available/(d[6]*d[7]));stride=d[6]*d[7];}
    else if(scalar_arg){input_type=method==13?1:method==15?2:3;available=1;stride=1;}
    else if(method==14||method==16||method==18){input_type=method==14?1:method==16?2:3;source=a[3];available=a[4];elements=std::min(d[8],available/(d[6]*d[7]));stride=d[6]*d[7];}
    else if(method==19||method==20){source=a[3];elements=method==19?1:std::min(d[8],a[4]);stride=4;available=elements*stride;}
    else if(matrix){source=a[3];elements=(method==21||method==24)?1:std::min(d[8],a[4]);stride=16;available=elements*stride;}
    else unsupported("CTAB setter method");
    if(!elements)return S_OK;if(!scalar_arg&&!source)return D3DERR_INVALIDCALL;
    if(scalar_arg&&(d[6]!=1||d[7]!=1))return D3DERR_INVALIDCALL;
    if(indirect)pointer(source,uint64_t(elements)*4);else if(!scalar_arg)pointer(source,uint64_t(available)*4);
    uint32_t per=d[1]==0?d[6]*d[7]:d[4]==2?d[6]:d[4]==3?d[7]:1;
    if(uint64_t(per)*elements>d[3])return D3DERR_INVALIDCALL;
    auto& output=table->upload_scratch;
    output.assign(per*elements*(d[1]==0?1:4),0);
    for(uint32_t e=0;e<elements;++e){uint32_t start=indirect?read(source+e*4):source+e*stride*4;
        for(uint32_t r=0;r<d[6];++r)for(uint32_t col=0;col<d[7];++col){
            uint32_t index=matrix?(transpose?col*4+r:r*4+col):r*d[7]+col;
            uint32_t raw=scalar_arg?scalar:read(start+index*4);double value=input_type==3?float_value(raw):input_type==1?double(raw!=0):double(int32_t(raw));
            uint32_t out_index=d[1]==0?e*per+r*d[7]+col:(e*per+(d[4]==3?col:d[4]==2?r:0))*4+(d[4]==3?r:col);
            if(d[1]==2)output[out_index]=float_bits(float(value));else if(d[1]==1){if(value<std::numeric_limits<int32_t>::min()||value>std::numeric_limits<int32_t>::max()||value!=value)return D3DERR_INVALIDCALL;output[out_index]=uint32_t(int32_t(value));}else output[out_index]=value!=0;
        }
    }
    return constant_upload(table,device,*c,output.data(),per*elements);
}
std::vector<unsigned char> shader_file(const std::string& path){
    FILE *file=std::fopen(path.c_str(),"rb");if(!file)return {};
    if(std::fseek(file,0,SEEK_END)){std::fclose(file);return {};}
    long size=std::ftell(file);if(size<0||size>8*1024*1024){std::fclose(file);return {};}
    std::rewind(file);std::vector<unsigned char> bytes(size);bool ok=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);return ok?bytes:std::vector<unsigned char>{};
}
std::string cached_shader_request(const std::vector<unsigned char>& request){
    // Exact request bytes remain the key. Index each immutable private cache
    // directory once instead of rereading every HLSL source for every shader.
    static std::map<std::vector<unsigned char>,std::string> cache;
    static std::map<std::string,bool> indexed;
    auto known=cache.find(request);if(known!=cache.end())return known->second;
    DIR *directory=opendir("shader-cache");
    if(directory){while(auto entry=readdir(directory)){std::string name(entry->d_name);
        if(name.size()!=64||name.find_first_not_of("0123456789abcdef")!=std::string::npos||indexed.count(name))continue;
        std::string prefix="shader-cache/"+name+"/";auto captured=shader_file(prefix+"request.bin");
        if(captured.empty())continue;
        cache.emplace(std::move(captured),prefix);indexed.emplace(name,true);
    }closedir(directory);}
    known=cache.find(request);return known==cache.end()?std::string{}:known->second;
}
uint32_t compiled_shader(const uint32_t *args){
    if(!args[7])return D3DERR_INVALIDCALL;write(args[7],0);if(args[8])write(args[8],0);if(args[9])write(args[9],0);
    auto request=shader_request(args);std::string match=cached_shader_request(request);
    if(match.empty()){
        capture_shader(args,request);
        // Explicit development opt-in: wait for the host to compile this exact
        // capture and atomically install it. No shader is substituted, and a
        // normal installation still stops immediately at an unknown request.
        if(access("shader-cache-wait",F_OK)==0){
            std::fprintf(stderr,"[d3dx] development cache wait (60 seconds maximum)\n");
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
            do{std::this_thread::sleep_for(std::chrono::milliseconds(250));match=cached_shader_request(request);}
            while(match.empty()&&std::chrono::steady_clock::now()<deadline);
        }
        if(match.empty())unsupported("D3DXCompileShader requires a compiled shader cache (request captured)");
    }
    auto shader=shader_file(match+"shader.bin"),ctab=shader_file(match+"constants.bin");
    if(shader.size()<8||shader.size()%4||ctab.size()<28)unsupported("invalid compiled shader cache");
    auto buffer=new GuestBuffer(shader.size());std::memcpy(pointer(buffer->data,buffer->size),shader.data(),shader.size());
    validate_shader(buffer->data);uint32_t table=args[9]?wrap(new GuestConstants(ctab),ConstantTable):0;
    write(args[7],wrap(buffer,Buffer));if(args[9])write(args[9],table);
    std::fprintf(stderr,"[d3dx] exact HLSL cache hit: %zu bytes, table=%08x\n",shader.size(),table);return S_OK;
}
