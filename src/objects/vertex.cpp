module;
#include <cstddef>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
export module vertex;

export struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;
    glm::vec3 normal;
    
    

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription description = {};
        description.setBinding(0)
        .setStride(sizeof(Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);
        return description;
    }

    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() 
    {
        return {{{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
                {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)},
                {2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)},
                {3, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)}
            }};
    }
};