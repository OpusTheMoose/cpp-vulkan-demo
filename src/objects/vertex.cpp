module;
#include "vulkan/vulkan.hpp"
#include <cstddef>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
export module vertex;

export struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription description = {};
        description.setBinding(0)
        .setStride(sizeof(Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);
        return description;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() 
    {
        return {{{0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)},
                {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)}}};
    }
};