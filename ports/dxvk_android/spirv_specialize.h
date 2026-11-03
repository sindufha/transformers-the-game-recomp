#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

bool tfSpirvSpecialize(const uint32_t *code,size_t words,
    const VkSpecializationInfo *specialization,std::vector<uint32_t>& result);
