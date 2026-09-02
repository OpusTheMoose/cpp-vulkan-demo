module;
#include <vector>
#include <glm/matrix.hpp>
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
                
        

    private:
        std::vector<Vertex> vertices;
        std::vector<u16> indices;
};

export struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};