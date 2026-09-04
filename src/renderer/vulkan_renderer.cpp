module;
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_vulkan.h"
#include <SDL3/SDL_video.h>
#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

export module vulkan_renderer;
import logger;
import types;
import window;
import file;
import vertex;
import mesh;
import camera;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr i32 FRAMES_IN_FLIGHT = 2;
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
                createUniformBuffers();
                createDescriptorPool();
                createDescriptorSets();
                createCommandBuffers();
                createSyncObjects();
                logger.log(LogLevel::INFO, "Vulkan renderer initialized");
            }
            catch(const std::exception &e)
            {
                logger.log(LogLevel::CRITICAL, e.what());

            }
        }
        auto upload(Mesh &mesh) -> void {
            createVertexBuffer(mesh);
            createIndexBuffer(mesh);
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

        auto drawFrame(Camera camera, std::vector<Mesh> &meshes) -> void {
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

            commandBuffers[frameIndex].reset();
            
            recordCommandBuffer(imageIndex, meshes);

            graphicsQueue.waitIdle();

            updateUniformBuffer(camera, frameIndex);

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


    private:
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
        constexpr vk::ApplicationInfo appInfo {
             "Hello Triangle",
             VK_MAKE_VERSION(1, 0, 0),
             "No Engine",
             VK_MAKE_VERSION(1, 0 , 0),
            vk::ApiVersion14};

     
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

    auto createSurface(Window& window) -> void {
        VkSurfaceKHR _surface;
        assert(window.getSDLWindow() != nullptr);

        if (SDL_Vulkan_CreateSurface(window.getSDLWindow(), *instance, nullptr, &_surface) == false)
        {
            throw std::runtime_error("Unable to create window surface for Vulkan " + std::string(SDL_GetError()));
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);

    }

    std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

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
                                                                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                  features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                  features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

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
                   vk::PhysicalDeviceVulkan13Features,
                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain;
         featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters = true;
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

    auto recordCommandBuffer(u32 imageIndex, std::vector<Mesh> &meshes) -> void
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
        
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);

        for (auto& mesh : meshes)
        {
            commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), glm::value_ptr(mesh.getModelMatrix()));
            commandBuffer.bindVertexBuffers(0, *mesh.vertexBuffer, {0}); 
            commandBuffer.bindIndexBuffer(*mesh.indexBuffer, 0, vk::IndexType::eUint16);
            commandBuffer.drawIndexed(static_cast<u32>(mesh.getIndices().size()), 1, 0, 0, 0);
           
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
           .setSubresourceRange(range);   // <-- the missing line

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

    auto copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size)
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

    
    void createVertexBuffer(Mesh &mesh)
    {
        vk::DeviceSize bufferSize = sizeof(Vertex) * mesh.getVertices().size();

        auto [stagingBuffer, stagingBufferMemory] =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, mesh.getVertices().data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(mesh.vertexBuffer, mesh.vertexBufferMemory) =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, mesh.vertexBuffer, bufferSize);
    }

    void createIndexBuffer(Mesh &mesh) {

         vk::DeviceSize bufferSize = sizeof(u16) * mesh.getIndices().size();

        auto [stagingBuffer, stagingBufferMemory] =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, mesh.getIndices().data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(mesh.indexBuffer, mesh.indexBufferMemory) =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, mesh.indexBuffer, bufferSize);

    }
   

   std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
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
        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
        buffer.bindMemory(*bufferMemory, 0);
        return {std::move(buffer), std::move(bufferMemory)};
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
        vk::DescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);

        vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.setBindingCount(1)
        .setPBindings(&uboLayoutBinding);

        descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);

    }

    auto createUniformBuffers() -> void {
        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
            auto [buffer, bufferMem]  = createBuffer(
            bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            uniformBuffers.emplace_back(std::move(buffer));
            uniformBuffersMemory.emplace_back(std::move(bufferMem));
            uniformBuffersMapped.emplace_back( uniformBuffersMemory.back().mapMemory(0, bufferSize));
        }
    }

    auto updateUniformBuffer(Camera& camera, u32 currentImage) -> void {
       
        UniformBufferObject ubo{};
        
        ubo.view = camera.getViewMatrix(); 
        ubo.proj = camera.getProjMatrix();

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    auto createDescriptorPool() -> void {
        vk::DescriptorPoolSize poolSize;
        poolSize.setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(FRAMES_IN_FLIGHT);

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(FRAMES_IN_FLIGHT)
        .setPoolSizeCount(1)
        .setPPoolSizes(&poolSize);

        descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
        


    }
    auto createDescriptorSets() -> void {
        std::vector<vk::DescriptorSetLayout> layouts(FRAMES_IN_FLIGHT, *descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.setDescriptorPool(descriptorPool)
        .setDescriptorSetCount(static_cast<u32>(layouts.size()))
        .setPSetLayouts(layouts.data());


        descriptorSets = device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo = {};
            bufferInfo.setBuffer(uniformBuffers[i])
            .setOffset(0)
            .setRange(sizeof(UniformBufferObject));

            vk::WriteDescriptorSet   descriptorWrite;
            descriptorWrite.setDstSet(descriptorSets[i])
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setPBufferInfo(&bufferInfo);

            device.updateDescriptorSets(descriptorWrite, {});
                                
        }

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
    vk::raii::CommandPool  commandPool = nullptr; 

    vk::raii::Image depthImage = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView depthImageView = nullptr;

    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;


    //we want as many uniform buffers as frames in flight
    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

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