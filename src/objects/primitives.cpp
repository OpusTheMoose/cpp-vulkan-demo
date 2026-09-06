module;
#include <glm/ext/scalar_constants.hpp>
#include <vector>
#include <glm/glm.hpp>
export module sphere;
import mesh;
import vertex;
import types;

export class Sphere {
    public:
    Sphere(const float radius, const i32 subdivisions, const glm::vec3 color, const glm::vec3 pos)
    {

    mesh.setPosition(pos);
   
    mesh.upload(vertices, indices);

    }
    
    const auto& getVertices() {
        return vertices;
    }
    const auto& getIndices() {
        return indices;
    }
    auto& getMesh() {
        return mesh;
    }
    private:
        Mesh mesh;
        std::vector<Vertex> vertices;
        std::vector<u16> indices;
    
};
