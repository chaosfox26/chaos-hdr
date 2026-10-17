#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>



class VulkanHDR {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    VulkanHDR(VkInstance instance,VkPhysicalDevice physical,VkDevice device,
              PFN_vkGetInstanceProcAddr gipa,PFN_vkGetDeviceProcAddr gdpa,
              VkCommandBuffer command,VkExtent2D extent,VkImage input,const std::vector<VkImage>& targets);
    ~VulkanHDR();
    void record(VkCommandBuffer command,uint32_t index,bool enabled,uint64_t frame);
    void finishCapture();
};
