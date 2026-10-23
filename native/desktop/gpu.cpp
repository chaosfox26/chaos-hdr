#include "gpu.hpp"
#include "../vulkan_hdr.hpp"
#include <vulkan/vulkan.h>
#include <epoxy/gl.h>
#include <QFile>
#include <QCoreApplication>
#include <QVariant>
#include <array>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <libdrm/drm_fourcc.h>

namespace {
void check(VkResult r,const char* action){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(action)+" failed: "+std::to_string(r));}
void glCheck(const char* action){auto e=glGetError();
        if(e!=GL_NO_ERROR)throw std::runtime_error(std::string(action)+" OpenGL error: "+std::to_string(e));}
}
struct DesktopGPU::Impl {
    uint32_t width{},height{},family{};
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};
    VkPhysicalDeviceMemoryProperties memory{};
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};
    struct Image {VkImage image{};VkDeviceMemory memory{};VkImageView view{};GLuint texture{},glMemory{};} source,prepared,output;
    VkSemaphore ready{},done{};GLuint glReady{},glDone{};
    bool directMode=false,outputReleased=false,submitted=false,poisoned=false,retired=false;
    GLsync retirementFence{};
#ifdef TRUEHDR_DEVELOPMENT
    int completionTest=0;
#endif
    bool complete(){
#ifdef TRUEHDR_DEVELOPMENT
        if(completionTest&&submitted)vkGetFenceStatus(device,fence);
        if(completionTest==1)return false;
        if(completionTest==2)poisoned=true;
#endif
        if(poisoned)return false;
        if(!submitted)return true;
        auto result=vkGetFenceStatus(device,fence);
        if(result==VK_SUCCESS)return true;
        if(result!=VK_NOT_READY)poisoned=true;
        return false;
    }
    bool retirementReady(){
        retired=true;
        if(!complete())return false;
        if(!source.glMemory&&!output.glMemory&&!glReady&&!glDone)return true;
        if(!retirementFence){retirementFence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);if(!retirementFence){poisoned=true;return false;}glFlush();}
        auto result=glClientWaitSync(retirementFence,0,0);
        if(result==GL_WAIT_FAILED)poisoned=true;
        return result==GL_ALREADY_SIGNALED||result==GL_CONDITION_SATISFIED;
    }
    void queueSubmit(const VkSubmitInfo& info){
        submitted=true;auto result=vkQueueSubmit(queue,1,&info,fence);
        if(result!=VK_SUCCESS)poisoned=true;
        check(result,"Submit HDR commands");
    }
    VkSemaphore producerReady{};
    struct Imported {Image image;dev_t device{};ino_t inode{};DesktopGPU::DmaInput description;};
    std::vector<std::unique_ptr<Imported>> imported;
    VkDescriptorSetLayout descriptors{};VkDescriptorPool descriptorPool{};VkDescriptorSet set{};
    VkPipelineLayout layout{};VkPipeline pipeline{};VkSampler sampler{};
    std::unique_ptr<VulkanHDR> engine;uint64_t frames=0;
    VkQueryPool queries{};bool previousTimed=false;float timestampPeriod=1;
    DesktopGPU::Timings timings;
    uint32_t memoryType(uint32_t bits){
        for(uint32_t n=0;n<memory.memoryTypeCount;++n)if((bits&(1u<<n))&&(memory.memoryTypes[n].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))return n;
        throw std::runtime_error("No compatible GPU-local memory");
    }
    void image(Image& out,VkFormat format,bool shared){
        VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};create.pNext=shared?&external:nullptr;
        create.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;create.imageType=VK_IMAGE_TYPE_2D;create.format=format;
        create.extent={width,height,1};create.mipLevels=create.arrayLayers=1;create.samples=VK_SAMPLE_COUNT_1_BIT;
        create.tiling=VK_IMAGE_TILING_OPTIMAL;create.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if(shared)create.usage|=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        check(vkCreateImage(device,&create,nullptr,&out.image),"Create shared image");
        VkMemoryRequirements required;vkGetImageMemoryRequirements(device,out.image,&required);
        VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.image=out.image;
        VkExportMemoryAllocateInfo exported{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};exported.handleTypes=external.handleTypes;exported.pNext=&dedicated;
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.pNext=shared?static_cast<void*>(&exported):static_cast<void*>(&dedicated);
        alloc.allocationSize=required.size;alloc.memoryTypeIndex=memoryType(required.memoryTypeBits);
        check(vkAllocateMemory(device,&alloc,nullptr,&out.memory),"Allocate shared image");check(vkBindImageMemory(device,out.image,out.memory,0),"Bind image memory");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=out.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=shared?format:VK_FORMAT_R8G8B8A8_UINT;
        view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};check(vkCreateImageView(device,&view,nullptr,&out.view),"Create image view");
        if(shared){
            auto getFd=reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(device,"vkGetMemoryFdKHR"));
            VkMemoryGetFdInfoKHR info{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};info.memory=out.memory;info.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;int fd=-1;
            check(getFd(device,&info,&fd),"Export image");
            glCreateMemoryObjectsEXT(1,&out.glMemory);GLint yes=GL_TRUE;glMemoryObjectParameterivEXT(out.glMemory,GL_DEDICATED_MEMORY_OBJECT_EXT,&yes);
            glImportMemoryFdEXT(out.glMemory,required.size,GL_HANDLE_TYPE_OPAQUE_FD_EXT,fd);glCheck("Import shared memory");
            glGenTextures(1,&out.texture);glBindTexture(GL_TEXTURE_2D,out.texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_TILING_EXT,GL_OPTIMAL_TILING_EXT);
            glTexStorageMem2DEXT(GL_TEXTURE_2D,1,format==VK_FORMAT_R8G8B8A8_UNORM?GL_RGBA8:GL_RGB10_A2,width,height,out.glMemory,0);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D,0);glCheck("Create shared texture");
        }
    }
    void barrier(Image& image,uint32_t src,uint32_t dst,VkImageLayout old=VK_IMAGE_LAYOUT_GENERAL){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=old==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.oldLayout=old;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
        b.srcQueueFamilyIndex=src;b.dstQueueFamilyIndex=dst;b.image=image.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
    }
    void begin(){
        if(retired||poisoned)throw std::runtime_error("The HDR GPU context is no longer available");
        if(directMode){if(!complete())throw std::runtime_error("The HDR GPU queue is not ready");}
        else check(vkWaitForFences(device,1,&fence,VK_TRUE,5'000'000'000),"Wait for HDR test queue");
        if(previousTimed){uint64_t ticks[3]{};if(vkGetQueryPoolResults(device,queries,0,3,sizeof(ticks),ticks,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)==VK_SUCCESS){
            timings.lastPrepare=(ticks[1]-ticks[0])*timestampPeriod/1e6;timings.lastEngine=(ticks[2]-ticks[1])*timestampPeriod/1e6;
            timings.lastTotal=(ticks[2]-ticks[0])*timestampPeriod/1e6;
            timings.prepare+=timings.lastPrepare;timings.engine+=timings.lastEngine;timings.total+=timings.lastTotal;++timings.samples;}previousTimed=false;}
#ifdef TRUEHDR_DEVELOPMENT
        if(engine&&std::getenv("TRUEHDR_NATIVE_VERIFY"))engine->finishCapture();
#endif
        submitted=false;check(vkResetFences(device,1,&fence),"Reset fence");check(vkResetCommandBuffer(command,0),"Reset commands");VkCommandBufferBeginInfo start{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};check(vkBeginCommandBuffer(command,&start),"Begin HDR commands");
    }
    void submit(bool shared){
        check(vkEndCommandBuffer(command),"End HDR commands");VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&command;
        if(shared){info.waitSemaphoreCount=info.signalSemaphoreCount=1;info.pWaitSemaphores=&ready;info.pWaitDstStageMask=&stage;info.pSignalSemaphores=&done;}
        queueSubmit(info);
    }
    void semaphore(VkSemaphore& semaphore,GLuint& glSemaphore){
        VkExportSemaphoreCreateInfo exported{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};exported.handleTypes=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        VkSemaphoreCreateInfo create{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};create.pNext=&exported;check(vkCreateSemaphore(device,&create,nullptr,&semaphore),"Create GPU sync");
        VkSemaphoreGetFdInfoKHR info{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};info.semaphore=semaphore;info.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        int fd=-1;auto getFd=reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(device,"vkGetSemaphoreFdKHR"));check(getFd(device,&info,&fd),"Export GPU sync");
        glGenSemaphoresEXT(1,&glSemaphore);glImportSemaphoreFdEXT(glSemaphore,GL_HANDLE_TYPE_OPAQUE_FD_EXT,fd);glCheck("Import GPU sync");
    }
    void destroyImage(Image& image){
        if(image.view)vkDestroyImageView(device,image.view,nullptr);
        if(image.image)vkDestroyImage(device,image.image,nullptr);
        if(image.memory)vkFreeMemory(device,image.memory,nullptr);
    }
    Image& importInput(const DesktopGPU::DmaInput& input){
        if(input.planes!=1||input.width!=width||input.height!=height)
            throw std::runtime_error("Direct input requires an unscaled, single-plane RGB buffer");
        struct stat identity{};if(fstat(input.fd[0],&identity))throw std::runtime_error("Identify submitted GPU buffer");
        for(auto& entry:imported){auto& d=entry->description;
            if(entry->device==identity.st_dev&&entry->inode==identity.st_ino&&d.modifier==input.modifier&&d.format==input.format&&d.pitch==input.pitch&&d.offset==input.offset)return entry->image;
        }
        VkFormat format;
        switch(input.format){
        case DRM_FORMAT_XRGB8888:case DRM_FORMAT_ARGB8888:format=VK_FORMAT_B8G8R8A8_UNORM;break;
        case DRM_FORMAT_XBGR8888:case DRM_FORMAT_ABGR8888:format=VK_FORMAT_R8G8B8A8_UNORM;break;
        default:throw std::runtime_error("Submitted buffer is not supported 8-bit SDR RGB");
        }
        if(imported.size()>=8){destroyImage(imported.front()->image);imported.erase(imported.begin());}
        auto entry=std::make_unique<Imported>();entry->device=identity.st_dev;entry->inode=identity.st_ino;entry->description=input;
        auto& image=entry->image;
        try{
            VkSubresourceLayout plane{};plane.offset=input.offset[0];plane.rowPitch=input.pitch[0];
            VkImageDrmFormatModifierExplicitCreateInfoEXT modifier{VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT};modifier.drmFormatModifier=input.modifier;modifier.drmFormatModifierPlaneCount=1;modifier.pPlaneLayouts=&plane;
            VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;external.pNext=&modifier;
            VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};create.pNext=&external;create.imageType=VK_IMAGE_TYPE_2D;create.format=format;create.extent={width,height,1};create.mipLevels=create.arrayLayers=1;create.samples=VK_SAMPLE_COUNT_1_BIT;create.tiling=VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;create.usage=VK_IMAGE_USAGE_SAMPLED_BIT;
            check(vkCreateImage(device,&create,nullptr,&image.image),"Import submitted image layout");
            VkMemoryRequirements required;vkGetImageMemoryRequirements(device,image.image,&required);
            auto getProperties=reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(vkGetDeviceProcAddr(device,"vkGetMemoryFdPropertiesKHR"));
            VkMemoryFdPropertiesKHR properties{VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};check(getProperties(device,VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,input.fd[0],&properties),"Inspect submitted image memory");
            VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.image=image.image;
            auto type=memoryType(required.memoryTypeBits&properties.memoryTypeBits);
            VkImportMemoryFdInfoKHR importedFd{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR};importedFd.pNext=&dedicated;importedFd.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;importedFd.fd=dup(input.fd[0]);
            if(importedFd.fd<0)throw std::runtime_error("Duplicate submitted buffer handle");
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=&importedFd;allocation.allocationSize=required.size;allocation.memoryTypeIndex=type;
            auto result=vkAllocateMemory(device,&allocation,nullptr,&image.memory);if(result!=VK_SUCCESS)close(importedFd.fd);check(result,"Import submitted GPU memory");
            check(vkBindImageMemory(device,image.image,image.memory,0),"Bind submitted GPU memory");
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=image.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            check(vkCreateImageView(device,&view,nullptr,&image.view),"View submitted RGB buffer");
        }catch(...){destroyImage(image);throw;}
        imported.push_back(std::move(entry));return imported.back()->image;
    }
    void record(float sharpen,float ignoreGrain,bool timing,bool hdr){
        if(timing){
            if(!queries){VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};info.queryType=VK_QUERY_TYPE_TIMESTAMP;info.queryCount=3;check(vkCreateQueryPool(device,&info,nullptr,&queries),"Create desktop timing queries");}
            vkCmdResetQueryPool(command,queries,0,3);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,queries,0);
        }
        constexpr float sharpenGain=3.f;
        struct {uint32_t width,height;float intensity,grain;uint32_t hdr;} push{width,height,sharpen*sharpenGain,ignoreGrain,hdr?1u:0u};
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);
        vkCmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);vkCmdDispatch(command,(width+15)/16,(height+15)/16,1);
        if(timing)vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,queries,1);
        if(hdr)engine->record(command,0,true,++frames);
        if(timing){vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,queries,2);previousTimed=true;}
    }
    ~Impl(){
        if(!retirementReady()){DesktopGPU::quarantine();engine.release();return;}
        if(retirementFence)glDeleteSync(retirementFence);
        engine.reset();
        for(auto& entry:imported)destroyImage(entry->image);
        if(glReady)glDeleteSemaphoresEXT(1,&glReady);
        if(glDone)glDeleteSemaphoresEXT(1,&glDone);
        for(auto* image:{&source,&prepared,&output}){
            if(image->texture)glDeleteTextures(1,&image->texture);
        if(image->glMemory)glDeleteMemoryObjectsEXT(1,&image->glMemory);
            if(image->view)vkDestroyImageView(device,image->view,nullptr);
        if(image->image)vkDestroyImage(device,image->image,nullptr);
        if(image->memory)vkFreeMemory(device,image->memory,nullptr);
        }
        if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
        if(layout)vkDestroyPipelineLayout(device,layout,nullptr);
        if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
        if(descriptors)vkDestroyDescriptorSetLayout(device,descriptors,nullptr);
        if(sampler)vkDestroySampler(device,sampler,nullptr);
        if(ready)vkDestroySemaphore(device,ready,nullptr);
        if(done)vkDestroySemaphore(device,done,nullptr);
        if(producerReady)vkDestroySemaphore(device,producerReady,nullptr);
        if(fence)vkDestroyFence(device,fence,nullptr);
        if(queries)vkDestroyQueryPool(device,queries,nullptr);
        if(pool)vkDestroyCommandPool(device,pool,nullptr);
        if(device)vkDestroyDevice(device,nullptr);
        if(instance)vkDestroyInstance(instance,nullptr);
    }
};
DesktopGPU::DesktopGPU(uint32_t width,uint32_t height,bool direct):impl(std::make_unique<Impl>()){
    if(recoveryRequired())throw std::runtime_error("HDR GPU recovery requires signing out and back in");
    auto& s=*impl;s.width=width;s.height=height;s.directMode=direct;
    for(auto ext:{"GL_EXT_memory_object_fd","GL_EXT_semaphore_fd"})if(!epoxy_has_gl_extension(ext))throw std::runtime_error(std::string("Missing graphics interop extension: ")+ext);
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="TrueHDR Desktop";app.apiVersion=VK_API_VERSION_1_3;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pApplicationInfo=&app;check(vkCreateInstance(&create,nullptr,&s.instance),"Create desktop Vulkan instance");
    uint32_t count=0;check(vkEnumeratePhysicalDevices(s.instance,&count,nullptr),"List GPUs");std::vector<VkPhysicalDevice> devices(count);check(vkEnumeratePhysicalDevices(s.instance,&count,devices.data()),"List GPUs");
    GLubyte uuid[16]{};glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT,0,uuid);glCheck("Read compositor GPU identity");
    for(auto device:devices){VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p.pNext=&id;vkGetPhysicalDeviceProperties2(device,&p);
        if(p.properties.vendorID==0x10de&&std::memcmp(uuid,id.deviceUUID,16)==0){s.physical=device;break;}}
    if(!s.physical)throw std::runtime_error("No NVIDIA Vulkan GPU matches the desktop OpenGL GPU");
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(s.physical,&properties);s.timestampPeriod=properties.limits.timestampPeriod;
    uint32_t families=0;vkGetPhysicalDeviceQueueFamilyProperties(s.physical,&families,nullptr);std::vector<VkQueueFamilyProperties> queues(families);vkGetPhysicalDeviceQueueFamilyProperties(s.physical,&families,queues.data());
    bool found=false;for(uint32_t n=0;n<families;++n)if((queues[n].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)){s.family=n;found=true;break;}
    if(!found)throw std::runtime_error("No graphics/compute queue");
    std::vector<const char*> extensions{"VK_KHR_external_memory_fd","VK_KHR_external_semaphore_fd","VK_NVX_binary_import","VK_NVX_image_view_handle","VK_KHR_push_descriptor"};
    if(direct)for(auto name:{"VK_EXT_external_memory_dma_buf","VK_EXT_image_drm_format_modifier","VK_EXT_queue_family_foreign"})extensions.push_back(name);
    float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=s.family;queue.queueCount=1;queue.pQueuePriorities=&priority;
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&f12;vkGetPhysicalDeviceFeatures2(s.physical,&features);
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.pNext=&f12;device.pEnabledFeatures=&features.features;device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;device.enabledExtensionCount=extensions.size();device.ppEnabledExtensionNames=extensions.data();
    check(vkCreateDevice(s.physical,&device,nullptr,&s.device),"Create desktop Vulkan device");vkGetDeviceQueue(s.device,s.family,0,&s.queue);vkGetPhysicalDeviceMemoryProperties(s.physical,&s.memory);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=s.family;pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;check(vkCreateCommandPool(s.device,&pool,nullptr,&s.pool),"Create HDR command pool");
    VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};command.commandPool=s.pool;command.commandBufferCount=1;command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(vkAllocateCommandBuffers(s.device,&command,&s.command),"Allocate HDR commands");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fence.flags=VK_FENCE_CREATE_SIGNALED_BIT;check(vkCreateFence(s.device,&fence,nullptr,&s.fence),"Create HDR fence");
    if(!direct)s.image(s.source,VK_FORMAT_R8G8B8A8_UNORM,true);s.image(s.prepared,VK_FORMAT_R8G8B8A8_UNORM,false);s.image(s.output,VK_FORMAT_A2B10G10R10_UNORM_PACK32,true);
    s.semaphore(s.ready,s.glReady);s.semaphore(s.done,s.glDone);
    if(direct){VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};check(vkCreateSemaphore(s.device,&info,nullptr,&s.producerReady),"Create submitted-frame acquire semaphore");}
    VkDescriptorSetLayoutBinding bindings[3]{{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo desc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};desc.bindingCount=3;desc.pBindings=bindings;check(vkCreateDescriptorSetLayout(s.device,&desc,nullptr,&s.descriptors),"Create prepare descriptors");
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,20};VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.setLayoutCount=1;layout.pSetLayouts=&s.descriptors;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;check(vkCreatePipelineLayout(s.device,&layout,nullptr,&s.layout),"Create prepare layout");
    QFile shader(TRUEHDR_DESKTOP_SHADER);
    if(!shader.open(QIODevice::ReadOnly))throw std::runtime_error("Desktop shader missing");
    auto bytes=shader.readAll();
    VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};module.codeSize=bytes.size();module.pCode=reinterpret_cast<const uint32_t*>(bytes.constData());VkShaderModule code{};check(vkCreateShaderModule(s.device,&module,nullptr,&code),"Load prepare shader");
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=s.layout;pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,code,"main",nullptr};auto pipelineResult=vkCreateComputePipelines(s.device,{},1,&pipeline,nullptr,&s.pipeline);vkDestroyShaderModule(s.device,code,nullptr);check(pipelineResult,"Create prepare pipeline");
    VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2}};
    VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=2;dp.pPoolSizes=sizes;check(vkCreateDescriptorPool(s.device,&dp,nullptr,&s.descriptorPool),"Create prepare descriptor pool");
    VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};sa.descriptorPool=s.descriptorPool;sa.descriptorSetCount=1;sa.pSetLayouts=&s.descriptors;check(vkAllocateDescriptorSets(s.device,&sa,&s.set),"Allocate prepare set");
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.minFilter=sampler.magFilter=VK_FILTER_NEAREST;sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;check(vkCreateSampler(s.device,&sampler,nullptr,&s.sampler),"Create prepare sampler");
    VkDescriptorImageInfo infos[3]{{s.sampler,s.source.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,s.prepared.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,s.output.view,VK_IMAGE_LAYOUT_GENERAL}};
    VkWriteDescriptorSet writes[3]{};for(int n=0;n<3;++n){writes[n]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[n].dstSet=s.set;writes[n].dstBinding=n;writes[n].descriptorCount=1;writes[n].descriptorType=bindings[n].descriptorType;writes[n].pImageInfo=&infos[n];}vkUpdateDescriptorSets(s.device,direct?2:3,writes+(direct?1:0),0,nullptr);
    s.begin();for(auto* image:{&s.source,&s.prepared,&s.output})if(image->image)s.barrier(*image,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,VK_IMAGE_LAYOUT_UNDEFINED);
    s.engine=std::make_unique<VulkanHDR>(s.instance,s.physical,s.device,vkGetInstanceProcAddr,vkGetDeviceProcAddr,s.command,VkExtent2D{width,height},s.prepared.image,std::vector<VkImage>{s.output.image});
    for(auto* image:{&s.source,&s.output})if(image->image)s.barrier(*image,s.family,VK_QUEUE_FAMILY_EXTERNAL);
    s.submit(false);
}
DesktopGPU::~DesktopGPU()=default;
unsigned DesktopGPU::inputTexture()const{return impl->source.texture;}
unsigned DesktopGPU::outputTexture()const{return impl->output.texture;}
void DesktopGPU::convert(float sharpen,float ignoreGrain,bool timing,bool hdr){
    auto& s=*impl;GLuint textures[]{s.source.texture,s.output.texture};GLenum layouts[]{GL_LAYOUT_GENERAL_EXT,GL_LAYOUT_GENERAL_EXT};
    glSignalSemaphoreEXT(s.glReady,0,nullptr,2,textures,layouts);glFlush();glCheck("Signal desktop pixels ready");
    s.begin();for(auto* image:{&s.source,&s.output})s.barrier(*image,VK_QUEUE_FAMILY_EXTERNAL,s.family);
    s.barrier(s.prepared,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED);
    s.record(sharpen,ignoreGrain,timing,hdr);
    for(auto* image:{&s.source,&s.output})if(image->image)s.barrier(*image,s.family,VK_QUEUE_FAMILY_EXTERNAL);
    s.submit(true);glWaitSemaphoreEXT(s.glDone,0,nullptr,2,textures,layouts);glCheck("Wait for desktop HDR pixels");
}
void DesktopGPU::convertDirect(const DmaInput& input,float sharpen,float ignoreGrain,bool timing,bool hdr){
    auto& s=*impl;if(!s.directMode)throw std::runtime_error("Direct input was not initialized");
    s.begin();
#ifdef TRUEHDR_DEVELOPMENT
    if(s.completionTest==3){s.completionTest=0;throw std::runtime_error("Isolated recording failure");}
#endif
    auto& image=s.importInput(input);
    VkDescriptorImageInfo sampled{s.sampler,image.view,VK_IMAGE_LAYOUT_GENERAL};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=s.set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&sampled;vkUpdateDescriptorSets(s.device,1,&write,0,nullptr);
    int acquire=input.acquireFd>=0?dup(input.acquireFd):-1;
    if(input.acquireFd<0){dma_buf_export_sync_file request{};request.flags=DMA_BUF_SYNC_READ;if(ioctl(input.fd[0],DMA_BUF_IOCTL_EXPORT_SYNC_FILE,&request)==0)acquire=request.fd;else throw std::runtime_error("Export producer completion fence");}
    if(acquire<0)throw std::runtime_error("Duplicate producer completion fence");
    auto importSemaphore=reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(vkGetDeviceProcAddr(s.device,"vkImportSemaphoreFdKHR"));
    VkImportSemaphoreFdInfoKHR imported{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR};imported.semaphore=s.producerReady;imported.flags=VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;imported.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;imported.fd=acquire;
    auto result=importSemaphore(s.device,&imported);if(result!=VK_SUCCESS)close(acquire);check(result,"Import producer completion fence");
    s.barrier(image,VK_QUEUE_FAMILY_FOREIGN_EXT,s.family);s.barrier(s.output,VK_QUEUE_FAMILY_EXTERNAL,s.family);
    s.barrier(s.prepared,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED);
    s.record(sharpen,ignoreGrain,timing,hdr);
    s.barrier(image,s.family,VK_QUEUE_FAMILY_FOREIGN_EXT);s.barrier(s.output,s.family,VK_QUEUE_FAMILY_EXTERNAL);
    check(vkEndCommandBuffer(s.command),"End direct HDR commands");
    VkSemaphore waits[]{s.producerReady,s.ready};VkPipelineStageFlags stages[]{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&s.command;submit.waitSemaphoreCount=s.outputReleased?2:1;submit.pWaitSemaphores=waits;submit.pWaitDstStageMask=stages;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&s.done;
    s.queueSubmit(submit);
    GLuint texture=s.output.texture;GLenum layout=GL_LAYOUT_GENERAL_EXT;glWaitSemaphoreEXT(s.glDone,0,nullptr,1,&texture,&layout);glCheck("Acquire direct HDR output");
    s.outputReleased=false;
#ifdef TRUEHDR_DEVELOPMENT
    if(s.completionTest==4){s.completionTest=1;throw std::runtime_error("Isolated post-submit failure");}
#endif
}
void DesktopGPU::releaseOutput(){
    auto& s=*impl;if(!s.directMode||s.outputReleased)return;
    GLuint texture=s.output.texture;GLenum layout=GL_LAYOUT_GENERAL_EXT;glSignalSemaphoreEXT(s.glReady,0,nullptr,1,&texture,&layout);glFlush();glCheck("Release displayed HDR output");s.outputReleased=true;
}
DesktopGPU::Timings DesktopGPU::timings()const{
    auto result=impl->timings;if(result.samples){result.prepare/=result.samples;result.engine/=result.samples;result.total/=result.samples;}return result;
}

void DesktopGPU::setSettings(const PictureSettings& settings){impl->engine->setSettings(settings);}

bool DesktopGPU::supportsFormat(uint32_t format){return format==DRM_FORMAT_XRGB8888||format==DRM_FORMAT_ARGB8888||format==DRM_FORMAT_XBGR8888||format==DRM_FORMAT_ABGR8888;}
bool DesktopGPU::ready()const{bool ready=impl->complete();if(impl->poisoned)throw std::runtime_error("HDR GPU completion failed");return ready;}
bool DesktopGPU::retirementReady(){return impl->retirementReady();}
bool DesktopGPU::recoveryRequired(){return QCoreApplication::instance()&&QCoreApplication::instance()->property("chaosHdrGpuRecoveryRequired").toBool();}
void DesktopGPU::quarantine(){if(auto app=QCoreApplication::instance())app->setProperty("chaosHdrGpuRecoveryRequired",true);}
#ifdef TRUEHDR_DEVELOPMENT
void DesktopGPU::testCompletion(int mode){impl->completionTest=mode;}
#endif
bool DesktopGPU::producerReady(const DmaInput& input){
    int fd=input.acquireFd;bool owned=false;
    if(fd<0){dma_buf_export_sync_file request{};request.flags=DMA_BUF_SYNC_READ;
        if(ioctl(input.fd[0],DMA_BUF_IOCTL_EXPORT_SYNC_FILE,&request)!=0)return false;
        fd=request.fd;owned=true;}
    pollfd pending{fd,POLLIN,0};int result=poll(&pending,1,0);
    if(owned)close(fd);
    return result==1&&(pending.revents&POLLIN)&&!(pending.revents&(POLLERR|POLLNVAL));
}
