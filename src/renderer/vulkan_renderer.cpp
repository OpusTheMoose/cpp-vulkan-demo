module;


#include <cstddef>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <print>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


export module vulkan_renderer;
import logger;
import types;
import window;
import file;
import vertex;

import camera;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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


export struct DrawSubmission {
    Buffer const& vertexBuffer ;
    Buffer const& indexBuffer;
    Buffer const& instanceBuffer;
    Buffer const& lightBuffer;
    
    std::span<const glm::mat4> instanceData;
    std::span<const Light> lights;
    std::span<const vk::raii::DescriptorSet> descriptorSets;

    u32 indexCount, instanceCount;
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
                createDepthResource();
                createDescriptorSetLayout();
                createGraphicsPipeline();
                createCommandPool(); 
                createDescriptorPool(); 
                createCommandBuffers();
                createSyncObjects();
                logger.log(LogLevel::INFO, "Vulkan renderer initialized");
            }
            catch(const std::exception &e)
            {
                logger.log(LogLevel::CRITICAL, e.what());

            }
        }
   

        auto waitIdle() -> void {
            device.waitIdle();
        }

        auto recreateSwapChain() -> void {
            waitIdle();

            cleanupSwapChain();

            createSwapChain();
            createImageViews();
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
        

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto updateStorageBuffer( const Buffer &buffer, std::span<const T> data, u32 currentFrame, size_t bufferSize) -> void 
    {
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
    
        auto  stagingBuffer =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBuffer.buffersMemory.front().mapMemory(0, bufferSize);
        memcpy(dataStaging, data, bufferSize);
        stagingBuffer.buffersMemory.front().unmapMemory();

        auto vertexBuffer =
            createBuffer(bufferSize, 
                               vk::BufferUsageFlagBits::eVertexBuffer |
                                     vk::BufferUsageFlagBits::eTransferDst |
                                     vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                     vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                , vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer.buffers.back(), vertexBuffer.buffers.back(), bufferSize);

        return vertexBuffer;
    }

    auto createIndexBuffer(void* data, size_t bufferSize) -> Buffer {

        auto stagingBuffer =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBuffer.buffersMemory.front().mapMemory(0, bufferSize);
        memcpy(dataStaging, data, bufferSize);
        stagingBuffer.buffersMemory.front().unmapMemory();

        auto indexBuffer  =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer.buffers.back(), indexBuffer.buffers.back(), bufferSize);

        return indexBuffer;
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
        
        auto drawFrame(Camera camera, Buffer& uniformBuffer, DrawSubmission const& draw) -> void {
            auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		    if (fenceResult != vk::Result::eSuccess)
		    {
			    throw std::runtime_error("failed to wait for fence!");
		    }
		    device.resetFences(*inFlightFences[frameIndex]);

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

            UniformBufferObject cameraUBO = {};
            cameraUBO.proj = camera.getProjMatrix();
            cameraUBO.view = camera.getViewMatrix();

            updateUniformBuffer(camera, cameraUBO, uniformBuffer, frameIndex);
            updateStorageBuffer(draw.instanceBuffer, draw.instanceData, frameIndex, sizeof(glm::mat4) * 1000);
            updateStorageBuffer(draw.lightBuffer, draw.lights, frameIndex, sizeof(Light) * 50);
            commandBuffers[frameIndex].reset();
            recordCommandBuffer(draw, imageIndex);

            graphicsQueue.waitIdle();

           

            vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
            vk::SubmitInfo   submitInfo = {};
            submitInfo.setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*presentCompleteSemaphores[frameIndex])
            .setPWaitDstStageMask(&waitDestinationStageMask)
            .setCommandBufferCount(1)
            .setPCommandBuffers(&*commandBuffers[frameIndex])
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&*renderFinishedSemaphores[imageIndex]);

            graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

            vk::PresentInfoKHR PresentInfoKHR = {};
            PresentInfoKHR.setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*renderFinishedSemaphores[imageIndex])
            .setSwapchainCount(1)
            .setPSwapchains(&*swapChain)
            .setPImageIndices(&imageIndex);
        
            result = graphicsQueue.presentKHR(PresentInfoKHR);
            if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || window->isFramebufferResized())
            {
                window->setFramebufferResized(false);
                recreateSwapChain();
            }
            else
            {
                // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
                assert(result == vk::Result::eSuccess);
            }
            frameIndex = (frameIndex + 1)  % FRAMES_IN_FLIGHT;
            
        }


    auto allocateDescriptorSets() -> std::vector<vk::raii::DescriptorSet>
    {
        std::vector<vk::DescriptorSetLayout> layouts(FRAMES_IN_FLIGHT, *descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo;
        std::vector<vk::raii::DescriptorSet> descriptorSets;
        allocInfo.setDescriptorPool(descriptorPool)
        .setDescriptorSetCount(static_cast<u32>(layouts.size()))
        .setPSetLayouts(layouts.data());


        return device.allocateDescriptorSets(allocInfo);
    }


    auto updateDescriptorSets(
    vk::raii::DescriptorSet& set,
    Buffer const& uniformBuffer,
    Buffer const& passBuffer,
    Buffer const& lightBuffer,
    Buffer const& instanceBuffer,
    u32 frameIndex) -> void
    {
    std::array<vk::DescriptorBufferInfo, 4> bufferInfos = {
        vk::DescriptorBufferInfo{
            *uniformBuffer.buffers[frameIndex], 0, uniformBuffer.bufferSize
        },
        vk::DescriptorBufferInfo{
            *passBuffer.buffers[frameIndex], 0, passBuffer.bufferSize
        },
        vk::DescriptorBufferInfo{
            *lightBuffer.buffers[frameIndex], 0, lightBuffer.bufferSize
        },
        vk::DescriptorBufferInfo{
            *instanceBuffer.buffers[frameIndex], 0, instanceBuffer.bufferSize
        }
        };

        std::array<vk::WriteDescriptorSet, 4>   descriptorWrite;

         // UBO Descriptor
        descriptorWrite[0].setDstSet(set)
        .setDstBinding(0)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setPBufferInfo(&bufferInfos[0]);

        // Per pass buffer (unused currently)

       descriptorWrite[1].setDstSet(set)
        .setDstBinding(1)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setPBufferInfo(&bufferInfos[1]);

        // Material Buffer
        descriptorWrite[2].setDstSet(set)
        .setDstBinding(2)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setPBufferInfo(&bufferInfos[2]);

        // Instance buffer
        descriptorWrite[3].setDstSet(set)
        .setDstBinding(3)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setPBufferInfo(&bufferInfos[3]);

         device.updateDescriptorSets(descriptorWrite, {});

    };


    private:

    auto recordCommandBuffer( DrawSubmission const& draw, u32 imageIndex) -> void
    {
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

        vk::ClearValue  clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
       vk::RenderingAttachmentInfo attachmentInfo = {};
       attachmentInfo.setImageView(swapChainImageViews[imageIndex])
       .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
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
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
       
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *draw.descriptorSets[frameIndex], nullptr);

       // commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), glm::value_ptr(mesh.getModelMatrix()));
        commandBuffer.bindVertexBuffers(0, *draw.vertexBuffer.buffers[0], {0}); 
        commandBuffer.bindIndexBuffer(draw.indexBuffer.buffers[0], 0, vk::IndexType::eUint16);
        commandBuffer.drawIndexed(static_cast<u32>(draw.indexCount), draw.instanceCount, 0, 0, 0);
           
        

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
            logger.log(LogLevel::INFO, "Vulkan validation layers enabled");

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
        VkSurfaceKHR _surface;
        assert(window.getSDLWindow() != nullptr);

        if (SDL_Vulkan_CreateSurface(window.getSDLWindow(), *instance, nullptr, &_surface) == false)
        {
            throw std::runtime_error("Unable to create window surface for Vulkan " + std::string(SDL_GetError()));
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);

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
                                  features.template get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline;

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
    uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

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

         vk::ImageViewCreateInfo imageViewCreateInfo;
         imageViewCreateInfo.viewType         = vk::ImageViewType::e2D;
        imageViewCreateInfo.format           = swapChainSurfaceFormat.format;
        imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } ;
        vk::ImageSubresourceRange range;
        range.aspectMask = vk::ImageAspectFlagBits::eColor;
        range.levelCount = 1;
        range.layerCount = 1;

        for (auto &image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }

    }

    auto createGraphicsPipeline() -> void {
        auto shaderModule = createShaderModule(readFile("src/shaders/slang.spv"));
        vk::PipelineShaderStageCreateInfo vertShaderStagInfo;
        vertShaderStagInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStagInfo.module = shaderModule;
        vertShaderStagInfo.pName = "vertMain";

        vk::PipelineShaderStageCreateInfo fragShaderStagInfo;
        fragShaderStagInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStagInfo.module = shaderModule;
        fragShaderStagInfo.pName = "fragMain";

        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStagInfo, fragShaderStagInfo};

        auto  bindingDescription    = Vertex::getBindingDescription();
        auto  attributeDescriptions = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vertexInputInfo.setVertexBindingDescriptionCount(1)
        .setPVertexBindingDescriptions(&bindingDescription)
        .setVertexAttributeDescriptionCount(static_cast<u32>(attributeDescriptions.size()))
        .setPVertexAttributeDescriptions(attributeDescriptions.data());


        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        std::vector<vk::DynamicState>      dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        //vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};
        //vk::Rect2D scissor{vk::Offset2D{0, 0}, swapChainExtent};

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.depthClampEnable        = vk::False;
        rasterizer.rasterizerDiscardEnable = vk::False;
        rasterizer.polygonMode             = vk::PolygonMode::eFill;
        rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable         = vk::False;
        rasterizer.lineWidth               = 1.0f;

        vk::PipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.setDepthTestEnable(vk::True)
        .setDepthWriteEnable(vk::True)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(vk::False)
        .setStencilTestEnable(vk::False);

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
        multisampling.sampleShadingEnable = vk::False;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.blendEnable = vk::False;
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.logicOpEnable = vk::False;
        colorBlending.logicOp = vk::LogicOp::eCopy;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        vk::PushConstantRange pushConstantRange = {};
        pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eVertex)
        .setOffset(0)
        .setSize(sizeof(glm::mat4));

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.setSetLayoutCount(1)
        .setPSetLayouts(&*descriptorSetLayout)
        .setPushConstantRangeCount(1)
        .setPPushConstantRanges(&pushConstantRange);

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::PipelineRenderingCreateInfo pipelineRenderingInfo = {};
        pipelineRenderingInfo.setColorAttachmentCount(1)
        .setPColorAttachmentFormats(&swapChainSurfaceFormat.format)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain;
        pipelineCreateInfoChain.get().setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPDepthStencilState(&depthStencil)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPColorBlendState(&colorBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(pipelineLayout)
        .setRenderPass(nullptr);
        pipelineCreateInfoChain.assign(pipelineRenderingInfo);

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());


        logger.log(LogLevel::INFO, "Graphics pipeline created");
    }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo info;
        info.codeSize = code.size() * sizeof(char);
        info.pCode = reinterpret_cast<const u32*>(code.data());
        return vk::raii::ShaderModule(device, info); 

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
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    auto copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) -> void
    {
        vk::CommandBufferAllocateInfo allocInfo = {};
        allocInfo.setCommandPool(commandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
        vk::raii::CommandBuffer commandCopyBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());
        vk::CommandBufferBeginInfo info = {};
        info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        commandCopyBuffer.begin(info);

        commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
        commandCopyBuffer.end();

        vk::SubmitInfo submitInfo = {};
        submitInfo.setCommandBufferCount(1)
        .setPCommandBuffers(&*commandCopyBuffer);
        graphicsQueue.submit(submitInfo, nullptr);
        graphicsQueue.waitIdle();
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

    auto createDescriptorSetLayout() -> void {
      
        // SET 0 - Global and per-frame data. 
        vk::DescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

        // Per-pass data (unused)
        vk::DescriptorSetLayoutBinding unusedLayoutBinding = {};
        unusedLayoutBinding.setBinding(1)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);

        // Per-material data
         vk::DescriptorSetLayoutBinding materialLayoutBinding = {};
        materialLayoutBinding.setBinding(2)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

        // Per-object data
        vk::DescriptorSetLayoutBinding instanceLayoutBinding = {};
        instanceLayoutBinding.setBinding(3)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);


        std::array<vk::DescriptorSetLayoutBinding, 4> bindings = { uboLayoutBinding, unusedLayoutBinding, materialLayoutBinding, instanceLayoutBinding };

        vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.setBindingCount(static_cast<uint32_t>(bindings.size()))
            .setPBindings(bindings.data());

        descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);

    }

  

    auto updateUniformBuffer(Camera& camera, UniformBufferObject& uniformBufferObject, Buffer& ubo, u32 frameIndex) -> void {
       
        uniformBufferObject.view = camera.getViewMatrix(); 
        uniformBufferObject.proj = camera.getProjMatrix();

        memcpy(ubo.buffersMapped[frameIndex], &uniformBufferObject, sizeof(UniformBufferObject));
    }

    auto createDescriptorPool() -> void {
        std::array<vk::DescriptorPoolSize, 2> poolSizes;
        poolSizes[0].setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(FRAMES_IN_FLIGHT);
        // Create storage buffer at binding 1 for instancing
        // There's 3 descriptors per frame in flight
        poolSizes[1].setType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(FRAMES_IN_FLIGHT * 3);

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(FRAMES_IN_FLIGHT)
        .setPoolSizeCount(poolSizes.size())
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

	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::ImageCreateInfo imageInfo = {};
        imageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(format)
        .setExtent({width, height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
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
        std::tie(depthImage, depthImageMemory) = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
        depthImageView  = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
    }


    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    vk::raii::SurfaceKHR surface = nullptr; 
    vk::raii::Queue graphicsQueue = nullptr; 
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;

    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;

    // Raytracing pipeline
    vk::raii::PipelineLayout rtPipelineLayout = nullptr;
    vk::raii::Pipeline raytracingPipeline = nullptr;

    // Bottom and top level acceleration structures 
    std::vector<vk::raii::AccelerationStructureKHR> blasAccel;
    vk::raii::AccelerationStructureKHR tlasAccel = nullptr;

    Buffer sbtBuffer; // Shader binding table buffer
    std::vector<u8> shaderHandles; // opaque shader handles for the table
    vk::StridedDeviceAddressRangeKHR raygenRegion{};
    vk::StridedDeviceAddressRangeKHR missRegion{};
    vk::StridedDeviceAddressRangeKHR hitRegion{};
    vk::StridedDeviceAddressRangeKHR callableRegion{};





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

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR   swapChainSurfaceFormat;
    vk::Extent2D           swapChainExtent;

    std::vector<vk::raii::ImageView> swapChainImageViews;

    Logger logger;
    Window* window; 

};