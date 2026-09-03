module;
#include <glm/ext/matrix_transform.hpp>
#include <vector>
#include <glm/matrix.hpp>
#include <vulkan/vulkan_raii.hpp>
export module mesh;
import vertex;
import types;

export class Mesh {
    public:
        Mesh() {
         vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
            };

            indices = {
                0, 1, 2, 2, 3, 0
            };
        }

        std::vector<Vertex> getVertices() {
            return vertices;
        }

        std::vector<u16> getIndices() {
            return indices;
        }
        auto getModelMatrix() -> glm::mat4 {
            return glm::identity<glm::mat4>();
        }
                
       vk::raii::Buffer vertexBuffer = nullptr;
       vk::raii::DeviceMemory vertexBufferMemory = nullptr; 
       vk::raii::Buffer indexBuffer = nullptr;
       vk::raii::DeviceMemory indexBufferMemory = nullptr;
    private:
        std::vector<Vertex> vertices;
        std::vector<u16> indices;
};

export struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};