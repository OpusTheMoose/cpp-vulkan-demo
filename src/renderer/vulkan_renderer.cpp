module;
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_vulkan.h"
#include <SDL3/SDL_video.h>
#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

export module vulkan_renderer;
import logger;
import types;
import window;
import file;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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
                createGraphicsPipeline();
                createCommandPool();
                createCommandBuffer();
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

        auto drawFrame() -> void {
            auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
		    if (fenceResult != vk::Result::eSuccess)
		    {
			    throw std::runtime_error("failed to wait for fence!");
		    }
		    device.resetFences(*drawFence);

            auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);
            recordCommandBuffer(imageIndex);

            graphicsQueue.waitIdle();

            vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
            vk::SubmitInfo   submitInfo = {};
            submitInfo.setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*presentCompleteSemaphore)
            .setPWaitDstStageMask(&waitDestinationStageMask)
            .setCommandBufferCount(1)
            .setPCommandBuffers(&*commandBuffer)
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&*renderFinishedSemaphore);

            graphicsQueue.submit(submitInfo, *drawFence);

            vk::PresentInfoKHR PresentInfoKHR = {};
            PresentInfoKHR.setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*renderFinishedSemaphore)
            .setSwapchainCount(1)
            .setPSwapchains(&*swapChain)
            .setPImageIndices(&imageIndex);
        
            result = graphicsQueue.presentKHR(PresentInfoKHR);
            
            

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

    logger.log(LogLevel::INFO, "Swapchain created successfully");




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

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
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
        rasterizer.frontFace               = vk::FrontFace::eClockwise;
        rasterizer.depthBiasEnable         = vk::False;
        rasterizer.lineWidth               = 1.0f;

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

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.setSetLayoutCount(0);
        pipelineLayoutInfo.setPushConstantRangeCount(0);

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain;
        pipelineCreateInfoChain.get().setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
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

    auto createCommandBuffer() -> void {

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = commandPool; 
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;
        commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

    }

    auto recordCommandBuffer(u32 imageIndex) -> void
    {
        commandBuffer.begin({});
        transition_image_layout(imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 
            {}, 
            vk::AccessFlagBits2::eColorAttachmentWrite, 
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
            vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        vk::ClearValue  clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
       vk::RenderingAttachmentInfo attachmentInfo = {};
       attachmentInfo.setImageView(swapChainImageViews[imageIndex])
       .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
       .setLoadOp(vk::AttachmentLoadOp::eClear)
       .setStoreOp(vk::AttachmentStoreOp::eStore)
       .setClearValue(clearColor);

        vk::RenderingInfo renderingInfo = {};
        renderingInfo.setRenderArea(vk::Rect2D{{0, 0}, swapChainExtent});
        renderingInfo.setLayerCount(1);
        renderingInfo.setColorAttachmentCount(1);
        renderingInfo.setPColorAttachments(&attachmentInfo);

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

        commandBuffer.draw(3, 1, 0, 0);

        commandBuffer.endRendering();

        // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
    transition_image_layout(
        imageIndex,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageLayout::ePresentSrcKHR,
    vk::AccessFlagBits2::eColorAttachmentWrite,             // srcAccessMask
    {},                                                     // dstAccessMask
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // srcStage
    vk::PipelineStageFlagBits2::eBottomOfPipe               // dstStage
    );

    commandBuffer.end();

    }

    void transition_image_layout(
    uint32_t                imageIndex,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask)
    {
    vk::ImageSubresourceRange range = {};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
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
           .setImage(swapChainImages[imageIndex])
           .setSubresourceRange(range);   // <-- the missing line

    vk::DependencyInfo info = {};
    info.setDependencyFlags({})
        .setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&barrier);

    commandBuffer.pipelineBarrier2(info);
    }

    
    auto createSyncObjects() -> void {
        presentCompleteSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
        renderFinishedSemaphore  = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
        drawFence                = vk::raii::Fence(device, fenceInfo);
    }

    vk::raii::SurfaceKHR surface = nullptr; 
    vk::raii::Queue graphicsQueue = nullptr; 
    vk::raii::Device device = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::CommandPool  commandPool = nullptr;
    vk::raii::CommandBuffer commandBuffer = nullptr;
    vk::raii::Semaphore presentCompleteSemaphore = nullptr;
    vk::raii::Semaphore renderFinishedSemaphore  = nullptr;
    vk::raii::Fence     drawFence                = nullptr;


    u32 queueIndex = 0;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR   swapChainSurfaceFormat;
    vk::Extent2D           swapChainExtent;

    std::vector<vk::raii::ImageView> swapChainImageViews;

    Logger logger;
    Window* window;

};