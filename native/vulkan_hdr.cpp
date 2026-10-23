#define VK_NO_PROTOTYPES
#include "vulkan_hdr.hpp"
#include "nvsdk_ngx_vk.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
void vkChecked(VkResult value){if(value!=VK_SUCCESS)throw std::runtime_error("TrueHDR Vulkan resource failed: "+std::to_string(value));}
void ngxChecked(NVSDK_NGX_Result value,const char* action){if(value!=NVSDK_NGX_Result_Success)throw std::runtime_error(std::string(action)+" failed: "+std::to_string(uint32_t(value)));}


void logMessage(const char*,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature){}
}

struct VulkanHDR::Impl {
    VkDevice device{};VkExtent2D extent{};PFN_vkGetDeviceProcAddr gdpa{};
    VkPhysicalDeviceMemoryProperties memory{};
    VkImage input{};VkImageView inputView{};
    std::vector<VkImage> targets;std::vector<VkImageView> targetViews;
    NVSDK_NGX_Handle* handle{};NVSDK_NGX_Parameter* parameters{};bool initialized=false;
    PictureSettings settings;bool settingsApplied=false;
#ifdef TRUEHDR_DEVELOPMENT
    VkBuffer captureBuffer{};VkDeviceMemory captureMemory{};void* captureMapping{};bool capturePending=false;
#endif
    template<class T>T fn(const char* name){return reinterpret_cast<T>(gdpa(device,name));}
    VkDeviceMemory allocate(VkMemoryRequirements required,VkMemoryPropertyFlags flags){
        for(uint32_t n=0;n<memory.memoryTypeCount;++n)if((required.memoryTypeBits&(1u<<n))&&(memory.memoryTypes[n].propertyFlags&flags)==flags){
            VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=required.size;a.memoryTypeIndex=n;
            VkDeviceMemory result{};vkChecked(fn<PFN_vkAllocateMemory>("vkAllocateMemory")(device,&a,nullptr,&result));return result;
        }
        throw std::runtime_error("TrueHDR Vulkan memory type unavailable");
    }
    VkImageView view(VkImage image,VkFormat format){
        VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=image;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=format;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        VkImageView result{};vkChecked(fn<PFN_vkCreateImageView>("vkCreateImageView")(device,&v,nullptr,&result));return result;
    }
    void barrier(VkCommandBuffer command,VkImage image,VkImageLayout oldLayout,VkImageLayout newLayout,VkAccessFlags src,VkAccessFlags dst){
        VkImageMemoryBarrier m{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};m.srcAccessMask=src;m.dstAccessMask=dst;m.oldLayout=oldLayout;m.newLayout=newLayout;m.srcQueueFamilyIndex=m.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;m.image=image;m.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&m);
    }
    void controls(const PictureSettings& config){
        if(settingsApplied&&settings==config)return;
        for(auto pair:{std::pair{"TrueHDR.MaxLuminance",config.peak},std::pair{"TrueHDR.MiddleGray",config.middle},
                       std::pair{"TrueHDR.Contrast",config.contrast+100},std::pair{"TrueHDR.Saturation",config.saturation+100},
                       std::pair{"TrueHDR.DebandingOff",config.debanding?0:1}})parameters->Set(pair.first,unsigned(pair.second));
        settings=config;settingsApplied=true;
    }
    ~Impl(){
        if(handle)NVSDK_NGX_VULKAN_ReleaseFeature(handle);
        if(parameters)NVSDK_NGX_VULKAN_DestroyParameters(parameters);
        if(initialized)NVSDK_NGX_VULKAN_Shutdown1(device);
#ifdef TRUEHDR_DEVELOPMENT
        if(captureMapping)fn<PFN_vkUnmapMemory>("vkUnmapMemory")(device,captureMemory);
        if(captureBuffer)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,captureBuffer,nullptr);
        if(captureMemory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,captureMemory,nullptr);
#endif
        for(auto target:targetViews)fn<PFN_vkDestroyImageView>("vkDestroyImageView")(device,target,nullptr);
        if(inputView)fn<PFN_vkDestroyImageView>("vkDestroyImageView")(device,inputView,nullptr);
    }
};

VulkanHDR::VulkanHDR(VkInstance instance,VkPhysicalDevice physical,VkDevice device,PFN_vkGetInstanceProcAddr gipa,PFN_vkGetDeviceProcAddr gdpa,VkCommandBuffer command,VkExtent2D extent,VkImage input,const std::vector<VkImage>& targets):impl(std::make_unique<Impl>()){
    auto& s=*impl;s.device=device;s.extent=extent;s.gdpa=gdpa;s.targets=targets;s.input=input;
    auto getMemory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(gipa(instance,"vkGetPhysicalDeviceMemoryProperties"));getMemory(physical,&s.memory);
    std::wstring libs=QString::fromUtf8(TRUEHDR_RUNTIME_SITE "/nvvfx/libs").toStdWString();const wchar_t* paths[]{libs.c_str()};
    QString directory=QFileInfo(qEnvironmentVariable("TRUEHDR_PROFILE",QString::fromUtf8(TRUEHDR_DEFAULT_PROFILE))).absolutePath()+"/output/vulkan-ngx";QDir().mkpath(directory);auto logs=directory.toStdWString();
    NVSDK_NGX_FeatureCommonInfo info{};info.PathListInfo={paths,1};info.LoggingInfo={logMessage,NVSDK_NGX_LOGGING_LEVEL_OFF,true};
    ngxChecked(NVSDK_NGX_VULKAN_Init_with_ProjectID("64dc46e1-cd39-4a64-b750-227198d73e75",NVSDK_NGX_ENGINE_TYPE_CUSTOM,"0.1",logs.c_str(),instance,physical,device,gipa,gdpa,&info),"Initialize Vulkan NGX");s.initialized=true;
    ngxChecked(NVSDK_NGX_VULKAN_AllocateParameters(&s.parameters),"Allocate Vulkan controls");
    s.parameters->Set("Width",extent.width);s.parameters->Set("Height",extent.height);s.parameters->Set("OutWidth",extent.width);s.parameters->Set("OutHeight",extent.height);


    ngxChecked(NVSDK_NGX_VULKAN_CreateFeature(command,static_cast<NVSDK_NGX_Feature>(14),s.parameters,&s.handle),"Create Vulkan TrueHDR");
    s.inputView=s.view(s.input,VK_FORMAT_R8G8B8A8_UNORM);for(auto target:targets)s.targetViews.push_back(s.view(target,VK_FORMAT_A2B10G10R10_UNORM_PACK32));
    for(auto key:{"TrueHDR.InLeft","TrueHDR.InTop","TrueHDR.OutLeft","TrueHDR.OutTop"})s.parameters->Set(key,0u);
    for(auto key:{"TrueHDR.InRight","TrueHDR.OutRight"})s.parameters->Set(key,extent.width);
    for(auto key:{"TrueHDR.InBottom","TrueHDR.OutBottom"})s.parameters->Set(key,extent.height);
    s.controls({});
    std::fprintf(stderr,"TrueHDR: direct Vulkan engine ready, same NVIDIA feature, no CUDA handoff\n");
}
VulkanHDR::~VulkanHDR()=default;

void VulkanHDR::record(VkCommandBuffer command,uint32_t index,bool enabled,uint64_t frame){
    auto& s=*impl;if(!enabled)return;
    s.barrier(command,s.input,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    NVSDK_NGX_Resource_VK in{},out{};in.Resource.ImageViewInfo={s.inputView,s.input,{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1},VK_FORMAT_R8G8B8A8_UNORM,s.extent.width,s.extent.height};
    out.Resource.ImageViewInfo={s.targetViews.at(index),s.targets.at(index),{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1},VK_FORMAT_A2B10G10R10_UNORM_PACK32,s.extent.width,s.extent.height};out.ReadWrite=true;
    s.parameters->Set("Input1",static_cast<void*>(&in));s.parameters->Set("Output",static_cast<void*>(&out));
    ngxChecked(NVSDK_NGX_VULKAN_EvaluateFeature(command,s.handle,s.parameters,nullptr),"Evaluate Vulkan TrueHDR");
#ifdef TRUEHDR_DEVELOPMENT
    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={s.extent.width,s.extent.height,1};
    uint64_t captureFrame=std::getenv("TRUEHDR_CAPTURE_FRAME_NUMBER")?std::strtoull(std::getenv("TRUEHDR_CAPTURE_FRAME_NUMBER"),nullptr,10):2;
    if(frame!=captureFrame||!std::getenv("TRUEHDR_NATIVE_VERIFY"))return;
    auto bytes=VkDeviceSize(s.extent.width)*s.extent.height*4;
    if(!s.captureBuffer){
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes*2;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;vkChecked(s.fn<PFN_vkCreateBuffer>("vkCreateBuffer")(s.device,&b,nullptr,&s.captureBuffer));
        VkMemoryRequirements req{};s.fn<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(s.device,s.captureBuffer,&req);s.captureMemory=s.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkChecked(s.fn<PFN_vkBindBufferMemory>("vkBindBufferMemory")(s.device,s.captureBuffer,s.captureMemory,0));vkChecked(s.fn<PFN_vkMapMemory>("vkMapMemory")(s.device,s.captureMemory,0,bytes*2,0,&s.captureMapping));
    }
    s.barrier(command,s.input,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    s.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,s.input,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,s.captureBuffer,1,&copy);
    s.barrier(command,s.input,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    s.barrier(command,s.targets.at(index),VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    copy.bufferOffset=bytes;s.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,s.targets.at(index),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,s.captureBuffer,1,&copy);
    s.barrier(command,s.targets.at(index),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
    VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.size=VK_WHOLE_SIZE;b.buffer=s.captureBuffer;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    s.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);s.capturePending=true;
#else
    (void)frame;
#endif
}
void VulkanHDR::finishCapture(){
#ifdef TRUEHDR_DEVELOPMENT
    auto& s=*impl;if(!s.capturePending)return;s.capturePending=false;auto bytes=uint64_t(s.extent.width)*s.extent.height*4;
    QString directory=QString::fromLocal8Bit(std::getenv("TRUEHDR_NATIVE_VERIFY"));
    for(auto pair:{std::pair<const char*,uint64_t>{"input.rgba8",0},std::pair<const char*,uint64_t>{"output.rgb10a2",bytes}}){QFile file(directory+"/"+pair.first);if(file.open(QIODevice::WriteOnly))file.write(static_cast<const char*>(s.captureMapping)+pair.second,bytes);}
#endif
}

void VulkanHDR::setSettings(const PictureSettings& settings){impl->controls(settings);}
