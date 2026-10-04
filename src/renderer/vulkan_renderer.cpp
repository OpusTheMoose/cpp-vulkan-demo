module;


#include <cstddef>
#include <glm/ext/matrix_transform.hpp>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>
#include <imgui.h>
#include <tracy/Tracy.hpp>

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_vulkan.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <print>
#include <variant>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../utils/stb_image.h"


export module vulkan_renderer;
import logger;
import types;
import window;
import file;
import vertex;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// #ifdef NDEBUG
// constexpr bool enableValidationLayers = false;
// #else
constexpr bool enableValidationLayers = true;
// #endif

export constexpr i32 FRAMES_IN_FLIGHT = 2;


export struct Buffer {
    std::vector<vk::raii::Buffer> buffers;
    std::vector<vk::raii::DeviceMemory> buffersMemory;
    std::vector<void*> buffersMapped;
    size_t bufferSize = 0; 
};
export struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj; 
};

export struct Light {
    glm::vec3 pos;
    glm::vec3 color;
};

export struct InstanceData {
    glm::mat4x4 model;
    // For correct alignment 
    std::array<glm::vec4, 3> normal;

};
export enum DescriptorSetType {
    FRAME,
    PASS,
    MATERIAL,
    OBJECT
};


static_assert(sizeof(InstanceData) == 112);


export struct Descriptors {
    std::array<vk::raii::DescriptorSetLayout, 4> layouts = {
        nullptr, nullptr, nullptr, nullptr
    };
    std::vector<vk::raii::DescriptorSet> frameSets;
    std::vector<vk::raii::DescriptorSet> passSets;
    std::vector<vk::raii::DescriptorSet> materialSets;
    std::vector<vk::raii::DescriptorSet> instanceSets;

};

export struct Texture {
    Texture(std::string_view path) { createTexture(path); }
    auto createTexture(std::string_view path) -> void {
        pixels = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
        assert(pixels != NULL);
    }
    ~Texture() { if (pixels) stbi_image_free(pixels); }
    unsigned char* pixels = nullptr;
    i32 width, height, channels;
    Buffer textureBuffer;
    vk::raii::Image image = nullptr;    
    vk::raii::DeviceMemory imageMemory = nullptr;
    vk::raii::ImageView imageView = nullptr;
    vk::raii::Sampler textureSampler = nullptr;
};


export struct GraphicsPipeline {
    vk::raii::Pipeline pipeline = nullptr;
    vk::raii::PipelineLayout layout = nullptr;
};


using PushConstant = std::variant<std::monostate, glm::mat4>;

export struct DrawSubmission {
    GraphicsPipeline const &pipelineInfo; 
    Buffer const& vertexBuffer;
    Buffer const& indexBuffer;

    Buffer const& uniformBuffer;
    Buffer const& instanceBuffer;
    Buffer const& lightBuffer;
    
    std::span<const InstanceData> instanceData;
    std::span<const Light> lights;
    Descriptors const& descriptorSets;

    PushConstant pushConstant;
    glm::mat4 proj;
    glm::mat4 view;
    glm::vec3 cameraPosition;

    u32 indexCount, instanceCount;
};

export struct VulkanInfo {
    vk::raii::Instance& g_instance;
    vk::raii::PhysicalDevice g_physicalDevice;
    vk::raii::Device& g_device;
    VkSurfaceKHR& g_surface;

    vk::raii::Queue g_queue;
    u32 g_queueFamily;

    vk::raii::PipelineCache g_pipelineCache;
    vk::raii::DescriptorPool& g_descriptorPool;
    u32 g_minImageCount;
    u32 g_ImageCount;

    vk::raii::RenderPass g_renderPass;
    vk::SampleCountFlagBits g_msaaSamples;

    

};


export class VulkanRenderer{
    public:
        VulkanRenderer() {}
        VulkanRenderer(Window& window){
            try {
                this->window = &window;
                createInstance();
                createSurface(*this->window);
                pickPhysicalDevice();
                createLogicalDevice();
                createSwapChain();
                createImageViews();
                createColorResource();
                createDepthResource(); 
                createCommandPool(); 
                createDescriptorPool(); 
                createCommandBuffers();
                createSyncObjects();
                // uploadImageTexture(Texture("textures/2k_mercury.jpg"), vk::MemoryPropertyFlagBits::eDeviceLocal);
                logger.log(LogLevel::INFO, "Vulkan renderer initialized");
            }
            catch(const std::exception &e)
            {
                logger.log(LogLevel::CRITICAL, e.what());

            }
        }
   

        auto waitIdle() -> void {
            ZoneScopedN("Wait idle");
            device.waitIdle();
        }

        auto recreateSwapChain() -> void {
            ZoneScopedN("Recreate swap chain");
            waitIdle();

            cleanupSwapChain();

            createSwapChain();
            createImageViews();
            createColorResource();
            createDepthResource();
        }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo info;
        info.codeSize = code.size() * sizeof(char);
        info.pCode = reinterpret_cast<const u32*>(code.data());
        return vk::raii::ShaderModule(device, info); 

    }
    auto getCurrentDevice() -> const vk::raii::Device& {
        return this->device;
    }


       auto createDescriptorSetLayout(vk::ShaderStageFlags stageFlags, vk::DescriptorType descriptorSetType) ->  vk::raii::DescriptorSetLayout {

        // SET 0 - Global and per-frame data. 
        vk::DescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.setBinding(0)
        .setDescriptorType(descriptorSetType)
        .setDescriptorCount(1)
        .setStageFlags(stageFlags);

       
        vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.setBindingCount(1)
        .setPBindings(&layoutBinding);

        return vk::raii::DescriptorSetLayout(device, layoutInfo);

        }
    auto createStorageBuffer(size_t bufferSize) -> Buffer
    {
        Buffer instanceBuffer;

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            Buffer frameBuffer = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
            );

        instanceBuffer.buffers.emplace_back(
            std::move(frameBuffer.buffers.front())
            );

        instanceBuffer.buffersMemory.emplace_back(
            std::move(frameBuffer.buffersMemory.front())
            );

        instanceBuffer.buffersMapped.emplace_back(
            instanceBuffer.buffersMemory.back().mapMemory(0, bufferSize)
            );
        }

        instanceBuffer.bufferSize = bufferSize;
        return instanceBuffer;
    }

    auto getSwapSurfaceFormat()
    {
        return swapChainSurfaceFormat;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto updateStorageBuffer( const Buffer &buffer, std::span<const T> data, u32 currentFrame, size_t bufferSize) -> void 
    {
        ZoneScopedN("Update storage buffer");
        if (currentFrame >= buffer.buffersMapped.size())
        {
            throw std::out_of_range("Invalid frame index");
        }

        const size_t dataSize = data.size_bytes();
        if (dataSize > bufferSize){
            throw std::runtime_error("Storage buffer data is too large");
        }
           
        if (dataSize != 0) {
            memcpy(
                buffer.buffersMapped[currentFrame],
                data.data(),
                dataSize
            );
        }

    }

    auto createVertexBuffer(void* data, size_t bufferSize) -> Buffer
    {
        return
        createStagingBuffer(data, bufferSize, 
                               vk::BufferUsageFlagBits::eVertexBuffer |
                                     vk::BufferUsageFlagBits::eTransferDst |
                                     vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                     vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);

    }

    auto createIndexBuffer(void* data, size_t bufferSize) -> Buffer {

        return createStagingBuffer(data, bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    }

    auto createStagingBuffer(void* data, size_t bufferSize, vk::BufferUsageFlags flags) -> Buffer {
        auto stagingBuffer =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBuffer.buffersMemory.front().mapMemory(0, bufferSize);
        memcpy(dataStaging, data, bufferSize);
        stagingBuffer.buffersMemory.front().unmapMemory();

        auto newBuffer  =
            createBuffer(bufferSize, flags, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer.buffers.back(), newBuffer.buffers.back(), bufferSize);

        return newBuffer;

    }

    auto createUniformBuffers(size_t size) -> Buffer {
    
        Buffer uniformBuffer;

        for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; frame++)
        {
            Buffer frameBuffer = createBuffer(
            size, vk::BufferUsageFlagBits::eUniformBuffer, 
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

         uniformBuffer.buffers.emplace_back(
                std::move(frameBuffer.buffers.front())
            );

          uniformBuffer.buffersMemory.emplace_back(
                std::move(frameBuffer.buffersMemory.front())
            );

            // No buffer memory mapped yet!
          uniformBuffer.buffersMapped.emplace_back(
                uniformBuffer.buffersMemory.back().mapMemory(0, size)
            );

        }
        uniformBuffer.bufferSize = size;
        return uniformBuffer;
        
    }

    
        
        auto drawFrame(std::span<DrawSubmission> const& drawCalls, ImDrawData* drawData) -> void {
            ZoneScopedN("Draw Frame");
            {
                ZoneScopedN("Wait for fence");
                auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		        if (fenceResult != vk::Result::eSuccess)
		        {
			        throw std::runtime_error("failed to wait for fence!");
		        }   
		        device.resetFences(*inFlightFences[frameIndex]);
            }

            
                auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

                if (result == vk::Result::eErrorOutOfDateKHR)
                {
                    recreateSwapChain();
                    return;
                }
                if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
                {
                    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
                    throw std::runtime_error("Failed to acquire swap chain image!");
                }
            

                device.resetFences(*inFlightFences[frameIndex]);
            

            {
                ZoneScopedN("Handle Draw Calls")
                for (auto draw : drawCalls)
                {
                    UniformBufferObject cameraUBO = {};
                    cameraUBO.proj = draw.proj;
                    cameraUBO.view = draw.view; 
                    {
                        ZoneScopedN("Update Uniform Buffer");
                        updateUniformBuffer(cameraUBO, draw.uniformBuffer, frameIndex);
                    }

                    {
                        ZoneScopedN("Update storage buffers");
                        updateStorageBuffer(draw.instanceBuffer, draw.instanceData, frameIndex, draw.instanceBuffer.bufferSize);
                        updateStorageBuffer(draw.lightBuffer, draw.lights, frameIndex, draw.lightBuffer.bufferSize);
                    }

                }
            }
      
            commandBuffers[frameIndex].reset();
            recordCommandBuffer(drawCalls, drawData, imageIndex );

            {
                ZoneScopedN("GraphicsQueue wait idle");
                graphicsQueue.waitIdle();  
            }
           
            vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
            vk::SubmitInfo   submitInfo = {};
            submitInfo.setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*presentCompleteSemaphores[frameIndex])
            .setPWaitDstStageMask(&waitDestinationStageMask)
            .setCommandBufferCount(1)
            .setPCommandBuffers(&*commandBuffers[frameIndex])
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&*renderFinishedSemaphores[imageIndex]);

            {
                ZoneScopedN("Submit graphics info");
                graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);
            }

            vk::PresentInfoKHR PresentInfoKHR = {};
            PresentInfoKHR.setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*renderFinishedSemaphores[imageIndex])
            .setSwapchainCount(1)
            .setPSwapchains(&*swapChain)
            .setPImageIndices(&imageIndex);
        
            {
                ZoneScopedN("Graphics queue present KHR")
                    
                result = graphicsQueue.presentKHR(PresentInfoKHR);
            }
            if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || window->isFramebufferResized())
            {
                window->setFramebufferResized(false);
                ImGui_ImplVulkan_SetMinImageCount(minImageCount);
                recreateSwapChain();
            }
            else
            {
                // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
                assert(result == vk::Result::eSuccess);
            }
            frameIndex = (frameIndex + 1)  % FRAMES_IN_FLIGHT;
            
        }


    auto allocateDescriptorSets(vk::raii::DescriptorSetLayout const& layout, u32 count) -> std::vector<vk::raii::DescriptorSet>
    {
        std::vector<vk::DescriptorSetLayout> layouts(count, layout);
        vk::DescriptorSetAllocateInfo allocInfo;
        std::vector<vk::raii::DescriptorSet> descriptorSets;
        allocInfo.setDescriptorPool(descriptorPool)
            .setDescriptorSetCount(static_cast<u32>(layouts.size()))
            .setPSetLayouts(layouts.data());
        return device.allocateDescriptorSets(allocInfo);
    }

    

    auto updateFrameSet(vk::raii::DescriptorSet& set, Buffer const& buffer, u32 frameIndex) -> void
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setBuffer(*buffer.buffers[frameIndex])
            .setOffset(0)
            .setRange(buffer.bufferSize);

        vk::WriteDescriptorSet write;
        write.setDstSet(set)
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setPBufferInfo(&bufferInfo);

        device.updateDescriptorSets(write, {});
    }
    auto updatePassSet(vk::raii::DescriptorSet& set, Buffer const& buffer, u32 frameIndex) -> void
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setBuffer(*buffer.buffers[frameIndex])
            .setOffset(0)
            .setRange(buffer.bufferSize);

        vk::WriteDescriptorSet write;
        write.setDstSet(set)
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setPBufferInfo(&bufferInfo);

        device.updateDescriptorSets(write, {});
    }
    auto updateMaterialSet(vk::raii::DescriptorSet& set, Buffer const& buffer, Texture const& texture) -> void
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setBuffer(*buffer.buffers[0])
            .setOffset(0)
            .setRange(buffer.bufferSize);
        vk::DescriptorImageInfo imageInfo;
        imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(texture.imageView)
            .setSampler(texture.textureSampler);

        vk::WriteDescriptorSet write;
        write.setDstSet(set)
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setPBufferInfo(&bufferInfo)
            .setPImageInfo(&imageInfo);

        device.updateDescriptorSets(write, {});
    }
    auto updateObjectSet(vk::raii::DescriptorSet& set, Buffer const& buffer, u32 frameIndex) -> void
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setBuffer(*buffer.buffers[frameIndex])
            .setOffset(0)
            .setRange(buffer.bufferSize);

        vk::WriteDescriptorSet write;
        write.setDstSet(set)
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setPBufferInfo(&bufferInfo);

        device.updateDescriptorSets(write, {});
    }
   

    auto getRendererInfo() -> VulkanInfo {
        return {
            instance,
            physicalDevice,
            device,
            windowSurface,
            graphicsQueue,
            queueIndex,
            nullptr,
            descriptorPool,
            minImageCount,
            static_cast<u32>(swapChainImages.size()),
            nullptr, 
            vk::SampleCountFlagBits::e1

            

        };
    }

    auto uploadTextureVulkan(Texture& texture ){
        uploadImageTexture(texture, vk::MemoryPropertyFlagBits::eDeviceLocal);
    }

    private:

    vk::raii::CommandBuffer beginSingleTimeCommands() {
        vk::CommandBufferAllocateInfo allocInfo = {};
        allocInfo.setCommandPool(commandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

        vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
        vk::CommandBufferBeginInfo beginInfo = {};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer.begin(beginInfo);

        return commandBuffer;

    }
    auto endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer)
    {
        commandBuffer.end();

        vk::SubmitInfo submitInfo =  {};
        submitInfo.setCommandBufferCount(1)
        .setPCommandBuffers(&*commandBuffer);
        graphicsQueue.submit(submitInfo);
        graphicsQueue.waitIdle();
    }

    auto recordCommandBuffer(std::span<DrawSubmission> drawCalls,  ImDrawData* imguiDraw, u32 imageIndex) -> void
    {
        ZoneScopedN("Record Command Buffer");
        auto &commandBuffer = commandBuffers[frameIndex];
        commandBuffer.begin({});
        transition_image_layout(
            swapChainImages[imageIndex], 
            vk::ImageLayout::eUndefined, 
            vk::ImageLayout::eColorAttachmentOptimal, 
            {}, 
            vk::AccessFlagBits2::eColorAttachmentWrite, 
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor);

          transition_image_layout(
        *depthImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

        transition_image_layout(
        *colorImage,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::ImageAspectFlagBits::eColor);

        vk::ClearValue  clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);
       vk::RenderingAttachmentInfo attachmentInfo = {};
       attachmentInfo.
       setImageView(colorImageView)
       .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
       .setResolveMode(vk::ResolveModeFlagBits::eAverage)
       .setResolveImageView(swapChainImageViews[imageIndex])
       .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
       .setLoadOp(vk::AttachmentLoadOp::eClear)
       .setStoreOp(vk::AttachmentStoreOp::eStore)
       .setClearValue(clearColor);

       vk::RenderingAttachmentInfo depthAttachmentInfo = {};
       depthAttachmentInfo.setImageView(depthImageView)
       .setImageView(depthImageView)
       .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
       .setLoadOp(vk::AttachmentLoadOp::eClear)
       .setStoreOp(vk::AttachmentStoreOp::eDontCare)
       .setClearValue(clearDepth);

        vk::RenderingInfo renderingInfo = {};
        renderingInfo.setRenderArea(vk::Rect2D{{0, 0}, swapChainExtent});
        renderingInfo.setLayerCount(1);
        renderingInfo.setColorAttachmentCount(1);
        renderingInfo.setPColorAttachments(&attachmentInfo);
        renderingInfo.setPDepthAttachment(&depthAttachmentInfo);

        commandBuffer.beginRendering(renderingInfo);

        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

        for (auto const& draw : drawCalls)
        {
            ZoneScopedN("Handle draw calls");
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, draw.pipelineInfo.pipeline);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, draw.pipelineInfo.layout, 3, *draw.descriptorSets.instanceSets[frameIndex], nullptr);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, draw.pipelineInfo.layout, 0, *draw.descriptorSets.frameSets[frameIndex], nullptr);         
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, draw.pipelineInfo.layout, 2, *draw.descriptorSets.materialSets[frameIndex], nullptr);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, draw.pipelineInfo.layout, 1, *draw.descriptorSets.passSets[frameIndex], nullptr);         
               

            // Deduce type of push constant, if there is one
            std::visit([&](auto& arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (!std::is_same_v<T, std::monostate>)
                {
                    commandBuffer.pushConstants<T>(*draw.pipelineInfo.layout, vk::ShaderStageFlagBits::eVertex, 0, arg);
                }
            }, draw.pushConstant );
        
            commandBuffer.bindVertexBuffers(0, *draw.vertexBuffer.buffers[0], {0}); 
            commandBuffer.bindIndexBuffer(draw.indexBuffer.buffers[0], 0, vk::IndexType::eUint16);
            commandBuffer.drawIndexed(static_cast<u32>(draw.indexCount), draw.instanceCount, 0, 0, 0);
            
        }
        {
            ZoneScopedN("ImGUI Render Draw data");
            ImGui_ImplVulkan_RenderDrawData(imguiDraw, *commandBuffer);
        }
        

        commandBuffer.endRendering();

        // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
        transition_image_layout(
    swapChainImages[imageIndex],
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageLayout::ePresentSrcKHR,
    vk::AccessFlagBits2::eColorAttachmentWrite,             // srcAccessMask
    {},                                                     // dstAccessMask
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // srcStage
    vk::PipelineStageFlagBits2::eBottomOfPipe     ,          // dstStage
    vk::ImageAspectFlagBits::eColor
    );

  

        commandBuffer.end();

    }
    auto createInstance() -> void {

        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
            logger.log(LogLevel::WARNING, "Vulkan validation layers enabled. Disable this if it's a release build!");

        }
        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
		                                               [&layerProperties](auto const &requiredLayer) {
			                                               return std::ranges::none_of(layerProperties,
                                                                [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		                                               });
        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }

        vk::ApplicationInfo appInfo  = {};
        appInfo.pApplicationName = "Raytracing Demo";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = vk::ApiVersion14;
     
        auto requiredExtensions = getRequiredInstanceExtensions();

        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt =
        std::ranges::find_if(requiredExtensions,
                                [&extensionProperties](auto const &requiredExtension) {
                                    return std::ranges::none_of(extensionProperties,
                                                                [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
                                });
        if (unsupportedPropertyIt != requiredExtensions.end())
        {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
        }
 
        vk::InstanceCreateInfo createInfo{};
        createInfo.setPApplicationInfo(&appInfo);
        createInfo.enabledLayerCount = static_cast<u32>(requiredLayers.size());
        createInfo.ppEnabledLayerNames = requiredLayers.data();
        createInfo.enabledExtensionCount = static_cast<u32>(requiredExtensions.size()); 
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        this->instance = vk::raii::Instance(context, createInfo);

        logger.log(LogLevel::INFO, "Vulkan instance initialized");

        }
        

    std::vector<const char*> getRequiredInstanceExtensions()
    {
            u32 sdl3ExtensionCount = 0;
            auto sdl3Extensions = SDL_Vulkan_GetInstanceExtensions(&sdl3ExtensionCount);

            std::vector extensions(sdl3Extensions, sdl3Extensions + sdl3ExtensionCount);

            return extensions;
    }

    // TODO: Remove window reference
    auto createSurface(Window& window) -> void {
        
        assert(window.getSDLWindow() != nullptr);

        if (SDL_Vulkan_CreateSurface(window.getSDLWindow(), *instance, nullptr, &windowSurface) == false)
        {
            throw std::runtime_error("Unable to create window surface for Vulkan " + std::string(SDL_GetError()));
        }
        surface = vk::raii::SurfaceKHR(instance, windowSurface);

    }

    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    bool isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice )
    {
    // Check if the physicalDevice supports the Vulkan 1.3 API version
    bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

      // Check if any of the queue families support graphics operations
    auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
    bool supportsGraphics = std::ranges::any_of( queueFamilies, []( auto const & qfp ) { return !!( qfp.queueFlags & vk::QueueFlagBits::eGraphics ); } );

    // Check if all required physicalDevice extensions are available
    auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions =
    std::ranges::all_of( requiredDeviceExtension,
                         [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                         {
                           return std::ranges::any_of( availableDeviceExtensions,
                                                       [requiredDeviceExtension]( auto const & availableDeviceExtension )
                                                       { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
                         } );

  // Check if the physicalDevice supports the required features (shader draw parameters, dynamic rendering and extended dynamic state)
  auto features                 = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                                       vk::PhysicalDeviceVulkan11Features,
                                                                       vk::PhysicalDeviceVulkan13Features,
                                                                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                                                                       vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                                                                       vk::PhysicalDeviceRayTracingPipelineFeaturesKHR
                                                                       >();
  bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                  features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                  features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
                                  features.template get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure &&
                                  features.template get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline &&
                                  features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy;
                             

  // Return true if the physicalDevice meets all the criteria
  return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void pickPhysicalDevice()
    {
    std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
    auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice ) { return isDeviceSuitable( physicalDevice ); } );
    if ( devIter == physicalDevices.end() )
    {
     throw std::runtime_error( "failed to find a suitable GPU!" );
    }
    physicalDevice = *devIter;
    }

    auto createLogicalDevice() -> void {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        queueIndex = ~0;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                // found a queue family that supports both graphics and present
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == u32(~0))
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo {};
        float queuePriority = 0.5f;
        deviceQueueCreateInfo.setQueueFamilyIndex(graphicsIndex);
        deviceQueueCreateInfo.setQueueCount(1);
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

     

        // vk::PhysicalDeviceFeatures deviceFeatures;

        // Create a chain of feature structures
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                   vk::PhysicalDeviceVulkan11Features,
                   vk::PhysicalDeviceVulkan12Features,
                   vk::PhysicalDeviceVulkan13Features,
                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain;
         featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters = true;
        featureChain.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress = true;
         featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = true;
         featureChain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = true;
        featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = true;
        featureChain.get<vk::PhysicalDeviceFeatures2>().features.setSamplerAnisotropy(true);
    




        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
        deviceCreateInfo.setQueueCreateInfoCount(1);
        deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();
        
        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);

        logger.log(LogLevel::INFO, "Created logical device and graphics queue");
    }

    auto  createSwapChain()-> void {
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( *surface );
    swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
    minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

    std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
	vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo;
    swapChainCreateInfo.surface = *surface;
    swapChainCreateInfo.minImageCount = minImageCount;
    swapChainCreateInfo.imageFormat = swapChainSurfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = swapChainSurfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = swapChainExtent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode      = presentMode;
    swapChainCreateInfo.clipped          = true;
    swapChainCreateInfo.oldSwapchain    = nullptr;

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();

    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    const auto formatIt = std::ranges::find_if(
        availableFormats,
        [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
    {
        assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(availablePresentModes,
                               [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
               vk::PresentModeKHR::eMailbox :
               vk::PresentModeKHR::eFifo;
    }
    
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        int width, height;
        SDL_GetWindowSize(this->window->getSDLWindow(), &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    auto createImageViews() -> void {
        assert(swapChainImageViews.empty());

        //  vk::ImageViewCreateInfo imageViewCreateInfo;
        //  imageViewCreateInfo.viewType         = vk::ImageViewType::e2D;
        // imageViewCreateInfo.format           = swapChainSurfaceFormat.format;
        // imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } ;
        // vk::ImageSubresourceRange range;
        // range.aspectMask = vk::ImageAspectFlagBits::eColor;
        // range.levelCount = 1;
        // range.layerCount = 1;

        swapChainImageViews.reserve(swapChainImages.size());
        for ( auto &image: swapChainImages )
        {
            swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
        }

        // for (auto &image : swapChainImages)
        // {
        //     imageViewCreateInfo.image = image;
        //     swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        // }

    }

    auto createCommandPool() -> void {
        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueIndex;
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    auto createCommandBuffers() -> void {

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = commandPool; 
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = FRAMES_IN_FLIGHT;
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);

    }

    

    void transition_image_layout(
    vk::Image               image,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags image_aspect_flags)
    {
    vk::ImageSubresourceRange range = {};
    range.setAspectMask(image_aspect_flags)
         .setBaseMipLevel(0)
         .setLevelCount(1)
         .setBaseArrayLayer(0)
         .setLayerCount(1);

    vk::ImageMemoryBarrier2 barrier = {};
    barrier.setSrcStageMask(src_stage_mask)
           .setSrcAccessMask(src_access_mask)
           .setDstStageMask(dst_stage_mask)
           .setDstAccessMask(dst_access_mask)
           .setOldLayout(old_layout)
           .setNewLayout(new_layout)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange(range);   

    vk::DependencyInfo info = {};
    info.setDependencyFlags({})
        .setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&barrier);

    commandBuffers[frameIndex].pipelineBarrier2(info);
    }

    
    auto createSyncObjects() -> void {

        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
            vk::FenceCreateInfo fenceInfo;
            fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
			inFlightFences.emplace_back(device, fenceInfo);
		}
      
    }

    auto cleanupSwapChain() -> void {
        ZoneScopedN("Cleanup swapchain");
        swapChainImageViews.clear();
        
        depthImageView = nullptr;
        depthImage = nullptr;
        depthImageMemory = nullptr;

        swapChain = nullptr;
    }

    auto copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) -> void
    {
        vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
        commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size) );
        endSingleTimeCommands(std::move(commandCopyBuffer));
    }
    auto copyBufferToImage(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, u32 width, u32 height)
    {
        vk::BufferImageCopy region = {};
        region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
        .setImageOffset({0, 0, 0})
        .setImageExtent({width, height, 1});

        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
        
    }
 

    auto createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) -> Buffer
    {
        vk::BufferCreateInfo   bufferInfo = {};
        bufferInfo.setSize(size)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive);

    
        vk::raii::Buffer       buffer          = vk::raii::Buffer(device, bufferInfo);
        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));

        vk::MemoryAllocateFlagsInfo flagsInfo = {};
        if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            flagsInfo.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
            allocInfo.setPNext(&flagsInfo);
        }


        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
        buffer.bindMemory(*bufferMemory, 0);

        Buffer newBuffer;
        newBuffer.buffers.emplace_back(std::move(buffer));
        newBuffer.buffersMemory.emplace_back(std::move(bufferMemory)); 
        newBuffer.bufferSize = size;

        return newBuffer;
    }



    u32 findMemoryType(u32 typeFilter, vk::MemoryPropertyFlags properties)
    {

        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i))  && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");

    }

 

  

    auto updateUniformBuffer( UniformBufferObject& uniformBufferObject, const Buffer& ubo, u32 frameIndex) -> void {
        
        memcpy(ubo.buffersMapped[frameIndex], &uniformBufferObject, sizeof(UniformBufferObject));
    }

    auto createDescriptorPool() -> void {
         std::array<vk::DescriptorPoolSize, 5> poolSizes = {
        vk::DescriptorPoolSize{
            vk::DescriptorType::eUniformBuffer,
            FRAMES_IN_FLIGHT
        },
        vk::DescriptorPoolSize{
            vk::DescriptorType::eStorageBuffer,
            FRAMES_IN_FLIGHT * 3
        },
        vk::DescriptorPoolSize{
            vk::DescriptorType::eCombinedImageSampler,
            2
        },
        vk::DescriptorPoolSize{
            vk::DescriptorType::eSampledImage,
            100
        },
        vk::DescriptorPoolSize{
            vk::DescriptorType::eSampler,
            100
        }
        };
        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(FRAMES_IN_FLIGHT * 10)
        .setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
        .setPPoolSizes(poolSizes.data());

      

        descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
        


    }
 

    vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags)
	{
        vk::ImageSubresourceRange range = {};
        range.setAspectMask(aspectFlags)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);

		vk::ImageViewCreateInfo viewInfo = {};
        viewInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(format)
        .setSubresourceRange(range);

		return vk::raii::ImageView(device, viewInfo);
	}

	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::ImageCreateInfo imageInfo = {};
        imageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(format)
        .setExtent({width, height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(numSamples)
        .setTiling(tiling)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive);
        
		vk::raii::Image image = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));
        vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
        
		image.bindMemory(imageMemory, 0);

		return {std::move(image), std::move(imageMemory)};
	}

    auto createDepthResource() -> void {
        vk::Format depthFormat = vk::Format::eD32Sfloat;
        std::tie(depthImage, depthImageMemory) = createImage(swapChainExtent.width, swapChainExtent.height, msaaSamples, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
        depthImageView  = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
    }

    auto createColorResource() -> void {
        vk::Format colorFormat = swapChainSurfaceFormat.format;

        std::tie(colorImage, colorImageMemory) = createImage(
            swapChainExtent.width,
            swapChainExtent.height,
            msaaSamples,
            colorFormat,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor);
        

    }

      // call this after all the other ^^^ stuff
    auto createTextureImageView(Texture& texture) {
        texture.imageView = createImageView(texture.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);

    }

    auto createTextureSampler(Texture &texture) {
        vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(true)
        .setMaxAnisotropy(properties.limits.maxSamplerAnisotropy)
        .setCompareEnable(vk::False)
        .setCompareOp(vk::CompareOp::eAlways)
        .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
        .setUnnormalizedCoordinates(vk::False)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMipLodBias(0.0f)
        .setMinLod(0.0f)
        .setMaxLod(0.0f);

        texture.textureSampler = vk::raii::Sampler(device, samplerInfo);
        
    }


    auto uploadImageTexture(Texture &texture, vk::MemoryPropertyFlagBits properties  ) -> void {
    
        vk::DeviceSize imageSize = texture.width * texture.height * 4;
		texture.textureBuffer = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = texture.textureBuffer.buffersMemory.back().mapMemory(0, imageSize);
        memcpy(data, texture.pixels, imageSize );
        texture.textureBuffer.buffersMemory.back().unmapMemory();


        vk::ImageCreateInfo imageInfo ={};
        imageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Srgb)
        .setExtent({static_cast<u32>(texture.width), static_cast<u32>(texture.height), 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive);

         texture.image = vk::raii::Image(device, imageInfo);

        vk::MemoryRequirements memRequirements = texture.image.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));
        texture.imageMemory = vk::raii::DeviceMemory(device, allocInfo);
        texture.image.bindMemory(texture.imageMemory, 0);

        vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
        transitionImageLayout(commandBuffer, texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        copyBufferToImage(commandBuffer, texture.textureBuffer.buffers[0], texture.image, static_cast<u32>(texture.width), static_cast<u32>(texture.height)) ;
        transitionImageLayout(commandBuffer, texture.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        endSingleTimeCommands(std::move(commandBuffer));

        createTextureImageView(texture);
        createTextureSampler(texture);
      
        
    }
  
    auto transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) -> void {
         vk::ImageSubresourceRange range = {};
        range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setLayerCount(1);
        
        vk::ImageMemoryBarrier barrier = {};
        barrier.setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setImage(image)
        .setSubresourceRange(range);

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
        barrier.srcAccessMask = {};
         barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage      = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
        throw std::invalid_argument("unsupported layout transition!");
        }
        commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);

        

    }

    


    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    vk::raii::SurfaceKHR surface = nullptr; 
    vk::raii::Queue graphicsQueue = nullptr; 
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;

    vk::raii::Image colorImage = nullptr;
    vk::raii::DeviceMemory colorImageMemory = nullptr;
    vk::raii::ImageView colorImageView = nullptr;
    vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e8;
    

    vk::raii::CommandPool  commandPool = nullptr; 

    vk::raii::Image depthImage = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView depthImageView = nullptr;

    vk::raii::DescriptorPool descriptorPool = nullptr;
    
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;


    u32 queueIndex = 0;
    u32 frameIndex = 0;
    u32 minImageCount;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR   swapChainSurfaceFormat;
    vk::Extent2D           swapChainExtent;

    std::vector<vk::raii::ImageView> swapChainImageViews;

    Logger logger;
    Window* window; 
    VkSurfaceKHR windowSurface;

};

export struct ShaderStageConfig {
    std::vector<char> code;
    vk::ShaderStageFlagBits stage;
    std::string entryPoint ="main";
};

export struct PipelineConfig {
    std::vector<vk::DynamicState> dynamicStates{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    std::vector<vk::Format> colorAttachmentFormats;
    std::vector<ShaderStageConfig> shaderStages;

    std::vector<vk::VertexInputBindingDescription> vertexBindings;
    std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
    std::vector<vk::PushConstantRange> pushConstantRanges;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    vk::PipelineMultisampleStateCreateInfo multisampling;
    vk::PipelineViewportStateCreateInfo viewportInfo;
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    vk::PipelineColorBlendStateCreateInfo colorBlending;
    vk::PipelineRenderingCreateInfo renderingInfo;
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
};

export class PipelineBuilder {
    public:
        PipelineBuilder(VulkanRenderer& renderer_) :
        renderer(renderer_) {
            // pipelineConfig.dynamicStates.
            // setDynamicStateCount(static_cast<u32>(pipelineConfig.dynamicStates.size()))
            // .setPDynamicStates(pipelineConfig.dynamicStates.data());
        };


        PipelineBuilder& setShader(const std::string &shaderPath, vk::ShaderStageFlagBits stage, const std::string &name)
        {
            if (shaderPath.find(".spv") == std::string::npos)
            {
                logger.log(LogLevel::WARNING, "You forgot to use the .spv compiled binary!");
                return *this;
            }
            pipelineConfig.shaderStages.emplace_back(
                readFile(shaderPath),
                stage,
                name
            );
            return *this;

        } 
        PipelineBuilder& setVertexInputInfo(const vk::VertexInputBindingDescription& bindingDescription,
                                            std::span<const vk::VertexInputAttributeDescription> attributes)
        { 
             pipelineConfig.vertexBindings = {bindingDescription};
                 pipelineConfig.vertexAttributes.assign(
                attributes.begin(),
                attributes.end());

            pipelineConfig.vertexInputInfo
            .setVertexBindingDescriptionCount(
            static_cast<u32>(pipelineConfig.vertexBindings.size()))
            .setPVertexBindingDescriptions(
            pipelineConfig.vertexBindings.data())
            .setVertexAttributeDescriptionCount(
            static_cast<u32>(pipelineConfig.vertexAttributes.size()))
            .setPVertexAttributeDescriptions(
            pipelineConfig.vertexAttributes.data());

            return *this;
        }

        PipelineBuilder& setInputAssembly(vk::PrimitiveTopology topology) { pipelineConfig.inputAssemblyInfo.setTopology(topology); return *this; }

        PipelineBuilder& setDepthTest(vk::Bool32 state) {
            pipelineConfig.depthStencil.setDepthTestEnable(state)
            // Depth writes are disabled whenever depth testing is disabled
            .setDepthWriteEnable(vk::True)
            .setDepthCompareOp(vk::CompareOp::eGreater)
            .setDepthBoundsTestEnable(vk::False);
            return *this;
        }
        PipelineBuilder& setStencilTest(vk::Bool32 state) {pipelineConfig.depthStencil.setStencilTestEnable(state); return *this;}
        PipelineBuilder& setViewportState(u32 viewportCount, u32 scissorCount)
        {
            pipelineConfig.viewportInfo.setScissorCount(scissorCount)
            .setViewportCount(viewportCount);
            return *this;
        }
        PipelineBuilder& setColorBlendAttachment(vk::Bool32 blendEnable, vk::ColorComponentFlags colorWriteMask) {
            pipelineConfig.colorBlendAttachment.setBlendEnable(blendEnable)
            .setColorWriteMask(colorWriteMask);
            return *this;
        }
        // Only use this AFTER setting the blend attachment
        PipelineBuilder& setColorBlendState() {
            pipelineConfig.colorBlending.setAttachmentCount(1)
            .setLogicOpEnable(vk::False)
            .setLogicOp(vk::LogicOp::eCopy)
            .setPAttachments(&pipelineConfig.colorBlendAttachment);
            return *this;

        }
        PipelineBuilder& setColorBlendFactors(
            vk::BlendFactor srcColor,
            vk::BlendFactor dstColor,
            vk::BlendOp colorOp = vk::BlendOp::eAdd,
            vk::BlendFactor srcAlpha = vk::BlendFactor::eOne,
            vk::BlendFactor dstAlpha = vk::BlendFactor::eZero,
            vk::BlendOp alphaOp = vk::BlendOp::eAdd)
        {
            pipelineConfig.colorBlendAttachment
            .setSrcColorBlendFactor(srcColor)
            .setDstColorBlendFactor(dstColor)
            .setColorBlendOp(colorOp)
            .setSrcAlphaBlendFactor(srcAlpha)
            .setDstAlphaBlendFactor(dstAlpha)
            .setAlphaBlendOp(alphaOp);

            return *this;
        }       
        PipelineBuilder& setDepthClampEnable(vk::Bool32 state) {pipelineConfig.rasterizer.setDepthClampEnable(state); return *this; }
        PipelineBuilder& setDiscardEnable   (vk::Bool32 state) {pipelineConfig.rasterizer.setRasterizerDiscardEnable(state); return *this;}
        PipelineBuilder& setPolygonMode     (vk::PolygonMode mode) {pipelineConfig.rasterizer.setPolygonMode(mode); return *this;}
        PipelineBuilder& setCullMode        (vk::CullModeFlagBits mode) {pipelineConfig.rasterizer.setCullMode(mode); return *this;}
        PipelineBuilder& setFrontFace       (vk::FrontFace face) {pipelineConfig.rasterizer.setFrontFace(face); return *this;}
        PipelineBuilder& setDepthBiasEnable (vk::Bool32 mode) {pipelineConfig.rasterizer.setDepthBiasEnable(mode); return *this; }
        PipelineBuilder& setDepthWrite      (vk::Bool32 state) {pipelineConfig.depthStencil.setDepthWriteEnable(state); return *this; }
        PipelineBuilder& setLineWidth       (f32 width) {pipelineConfig.rasterizer.setLineWidth(width); return *this; }

        PipelineBuilder& setMultisampleSamples(vk::SampleCountFlagBits flags) {pipelineConfig.multisampling.setRasterizationSamples(flags); return *this; }
        PipelineBuilder& setMultisampleShading(vk::Bool32 enable) {pipelineConfig.multisampling.setSampleShadingEnable(enable); return *this; }

        PipelineBuilder& setPipelineSetLayouts(std::span<const vk::DescriptorSetLayout> handles) {pipelineConfig.pipelineLayoutInfo.setSetLayouts(handles); return *this; }

        PipelineBuilder& clearShaders() {pipelineConfig.shaderStages.clear(); return *this;};
        PipelineBuilder& setRenderingColorAttachmentCount(u32 count) {pipelineConfig.pipelineRenderingInfo.setColorAttachmentCount(count); return *this; }
        PipelineBuilder& setColorAttachmentFormats(std::span<const vk::Format> colorFormats) {
            pipelineConfig.colorAttachmentFormats.assign(colorFormats.begin(), colorFormats.end());
            pipelineConfig.pipelineRenderingInfo.setPColorAttachmentFormats(pipelineConfig.colorAttachmentFormats.data()); 
            return *this; 
        }
        PipelineBuilder& setPushConstant(vk::ShaderStageFlagBits stageFlags, u32 size)
        {
            vk::PushConstantRange range = {};
            range.setStageFlags(stageFlags)
            .setOffset(0)
            .setSize(size);
            pipelineConfig.pushConstantRanges.emplace_back(range);
         
            return *this;
        }
        PipelineBuilder& setDepthAttachmentFormat(const vk::Format format) {pipelineConfig.pipelineRenderingInfo.setDepthAttachmentFormat(format); return *this; }
        auto rebindPointers() -> void {
             pipelineConfig.dynamicStateInfo
                .setDynamicStateCount(
            static_cast<u32>(pipelineConfig.dynamicStates.size()))
                .setPDynamicStates(
            pipelineConfig.dynamicStates.data());

             pipelineConfig.vertexInputInfo
                .setVertexBindingDescriptionCount(
            static_cast<u32>(pipelineConfig.vertexBindings.size()))
                .setPVertexBindingDescriptions(
            pipelineConfig.vertexBindings.data())
                .setVertexAttributeDescriptionCount(
            static_cast<u32>(pipelineConfig.vertexAttributes.size()))
                .setPVertexAttributeDescriptions(
            pipelineConfig.vertexAttributes.data());

                pipelineConfig.colorBlending
                .setPAttachments(&pipelineConfig.colorBlendAttachment);


                 pipelineConfig.pipelineRenderingInfo
                .setColorAttachmentCount(
            static_cast<u32>(pipelineConfig.colorAttachmentFormats.size()))
                .setPColorAttachmentFormats(
            pipelineConfig.colorAttachmentFormats.data());

            pipelineConfig.pipelineLayoutInfo
            .setPushConstantRangeCount(static_cast<u32>(pipelineConfig.pushConstantRanges.size()))
            .setPushConstantRanges(pipelineConfig.pushConstantRanges);

            
        }
       auto build() -> GraphicsPipeline {
            rebindPointers();
            vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain;


            std::vector<vk::raii::ShaderModule> shaderModules;
            std::vector<vk::PipelineShaderStageCreateInfo> stages;

            shaderModules.reserve(pipelineConfig.shaderStages.size());
            stages.reserve(pipelineConfig.shaderStages.size());

            for (auto const& shader : pipelineConfig.shaderStages)
            {
                shaderModules.emplace_back(renderer.createShaderModule(shader.code));

                vk::PipelineShaderStageCreateInfo stageInfo;
                stageInfo
                .setStage(shader.stage)
                .setModule(*shaderModules.back())
                .setPName(shader.entryPoint.c_str());

                stages.emplace_back(stageInfo);
            }
           
            vk::raii::PipelineLayout layout = vk::raii::PipelineLayout(renderer.getCurrentDevice(), pipelineConfig.pipelineLayoutInfo);
            pipelineCreateInfoChain.get().setStageCount(pipelineConfig.shaderStages.size())
            .setPInputAssemblyState(&pipelineConfig.inputAssemblyInfo)
            .setPStages(stages.data())
            .setPVertexInputState(&pipelineConfig.vertexInputInfo)
            .setLayout(layout)
            .setPRasterizationState(&pipelineConfig.rasterizer)
            .setPDepthStencilState(&pipelineConfig.depthStencil)
            .setPMultisampleState(&pipelineConfig.multisampling)
            .setPDynamicState(&pipelineConfig.dynamicStateInfo)
            .setPColorBlendState(&pipelineConfig.colorBlending)
            .setPViewportState(&pipelineConfig.viewportInfo)
            .setRenderPass(nullptr);
            pipelineCreateInfoChain.assign(pipelineConfig.pipelineRenderingInfo);

            vk::raii::Pipeline new_pipeline = vk::raii::Pipeline( renderer.getCurrentDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());

            return GraphicsPipeline{std::move(new_pipeline), std::move(layout)};
            }


         auto createFromExistingPipeline(const PipelineBuilder& source) -> PipelineBuilder& {
            pipelineConfig = source.pipelineConfig;
            rebindPointers();
            return *this;
        }

    private:
       VulkanRenderer& renderer;
       Logger logger;
       PipelineConfig pipelineConfig;
      
};