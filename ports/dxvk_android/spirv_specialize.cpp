/* Freeze the actual pipeline specialization values before removing dead
 * sampler branches. No sampler type is guessed or forced to 2D. */
#include "spirv_specialize.h"
#include <spirv-tools/optimizer.hpp>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <unistd.h>

// The NDK 27 optimizer does not fold constant OpBitFieldUExtract. DXSO uses
// exactly this operation for sampler type/depth/projection selectors. Reuse
// the result ID as a global OpConstant, preserving types and the ID bound.
static bool foldUnsignedExtracts(std::vector<uint32_t>& code) {
    constexpr uint32_t TypeInt=21,Constant=43,Function=54,UnsignedExtract=203;
    std::unordered_set<uint32_t> integerTypes;
    std::unordered_map<uint32_t,uint32_t> constants;
    for(size_t at=5;at<code.size();){uint32_t count=code[at]>>16,op=code[at]&65535;
        if(!count||count>code.size()-at)return false;
        if(op==TypeInt&&count==4&&code[at+2]==32)integerTypes.insert(code[at+1]);
        if(op==Constant&&count==4&&integerTypes.count(code[at+1]))constants[code[at+2]]=code[at+3];
        at+=count;
    }
    std::unordered_set<size_t> removed;std::vector<uint32_t> definitions;
    for(size_t at=5;at<code.size();){uint32_t count=code[at]>>16,op=code[at]&65535;
        if(op==UnsignedExtract&&count==6&&integerTypes.count(code[at+1])){
            auto base=constants.find(code[at+3]),offset=constants.find(code[at+4]),bits=constants.find(code[at+5]);
            if(base!=constants.end()&&offset!=constants.end()&&bits!=constants.end()&&offset->second<=32&&bits->second<=32-offset->second){
                uint32_t value=bits->second==0?0:bits->second==32?base->second:
                    (base->second>>offset->second)&((uint32_t(1)<<bits->second)-1);
                definitions.insert(definitions.end(),{(4u<<16)|Constant,code[at+1],code[at+2],value});
                constants[code[at+2]]=value;removed.insert(at);
            }
        }
        at+=count;
    }
    if(removed.empty())return true;
    std::vector<uint32_t> output(code.begin(),code.begin()+5);bool inserted=false;
    for(size_t at=5;at<code.size();){uint32_t count=code[at]>>16,op=code[at]&65535;
        if(op==Function&&!inserted){output.insert(output.end(),definitions.begin(),definitions.end());inserted=true;}
        if(!removed.count(at))output.insert(output.end(),code.begin()+at,code.begin()+at+count);
        at+=count;
    }
    if(!inserted)return false;code.swap(output);return true;
}

bool tfSpirvSpecialize(const uint32_t *code,size_t words,
    const VkSpecializationInfo *specialization,std::vector<uint32_t>& result) {
    if(!code||words<5||words>4*1024*1024||!specialization)return false;
    std::unordered_map<uint32_t,std::vector<uint32_t>> values;
    for(uint32_t i=0;i<specialization->mapEntryCount;++i){
        auto entry=specialization->pMapEntries[i];
        if(entry.size!=4||entry.offset>specialization->dataSize||4>specialization->dataSize-entry.offset)return false;
        uint32_t value;std::memcpy(&value,static_cast<const unsigned char*>(specialization->pData)+entry.offset,4);
        if(!values.emplace(entry.constantID,std::vector<uint32_t>{value}).second)return false;
    }
    auto consumer=[](spv_message_level_t,const char*,const spv_position_t& position,const char* message){
        std::fprintf(stderr,"[spirv] word %zu: %s\n",position.index,message);
    };
    spvtools::Optimizer specialize(SPV_ENV_VULKAN_1_1);specialize.SetMessageConsumer(consumer);
    specialize.RegisterPass(spvtools::CreateSetSpecConstantDefaultValuePass(values));
    specialize.RegisterPass(spvtools::CreateFreezeSpecConstantValuePass());
    specialize.RegisterPass(spvtools::CreateFoldSpecConstantOpAndCompositePass());
    std::vector<uint32_t> frozen;
    if(!specialize.Run(code,words,&frozen)||!foldUnsignedExtracts(frozen))return false;
    spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_1);optimizer.SetMessageConsumer(consumer);
    optimizer.RegisterPass(spvtools::CreateInlineExhaustivePass());
    optimizer.RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass());
    optimizer.RegisterPass(spvtools::CreateLocalSingleStoreElimPass());
    optimizer.RegisterPass(spvtools::CreateSSARewritePass());
    optimizer.RegisterPass(spvtools::CreateCCPPass());
    optimizer.RegisterPass(spvtools::CreateSimplificationPass());
    optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass());
    optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
    optimizer.RegisterPass(spvtools::CreateCFGCleanupPass());
    optimizer.RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());
    optimizer.RegisterPass(spvtools::CreateEliminateDeadConstantPass());
    // DXSO keeps register arrays in Private storage. The standard performance
    // pipeline promotes them and removes unused vector lanes/interface inputs
    // (including PointCoord in triangle shaders) before mobile compilation.
    optimizer.RegisterPerformancePasses();
    optimizer.RegisterPass(spvtools::CreateCompactIdsPass());
    if(!optimizer.Run(frozen.data(),frozen.size(),&result))return false;
    // Private, explicitly created directory opts into a bounded shader capture.
    // This has no effect on a normal installation.
    if(access("gpu-debug-shaders",F_OK)==0){
        static std::atomic<unsigned> sequence{0};unsigned id=sequence.fetch_add(1);
        if(id<128){
            char path[128];std::snprintf(path,sizeof(path),"gpu-debug-shaders/fragment-%03u.spv",id);
            if(FILE* file=std::fopen(path,"wb")){std::fwrite(result.data(),4,result.size(),file);std::fclose(file);}
        }
    }
    return true;
}
