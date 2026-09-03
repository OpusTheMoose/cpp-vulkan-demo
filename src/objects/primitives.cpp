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
    Sphere(const float radius, const i32 subdivisions)
    {
    const i32 stacks = std::max(3, subdivisions); 
    const i32 slices = std::max(3, subdivisions * 2); 

    mesh.setScale(glm::vec3(1.0)* radius);

    for (i32 i = 0; i <= stacks; ++i)
    {
        // phi: 0 at north pole, PI at south pole
        const float phi = static_cast<float>(i) / static_cast<float>(stacks) * static_cast<float>(M_PI);
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (i32 j = 0; j <= slices; ++j)
        {
            // theta: 0 to 2PI around the sphere
            const float theta = static_cast<float>(j) / static_cast<float>(slices) * 2.0f * static_cast<float>(M_PI);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            // Position on unit sphere, scaled by radius
            const float x = sinPhi * cosTheta;
            const float y = cosPhi;
            const float z = sinPhi * sinTheta;

            Vertex vert{};
            vert.pos = {x, y, z};
            vert.normal = {x, y, z};
            vert.color = {1.0, 1.0, 1.0};

            // UV: u wraps around, v goes from top (0) to bottom (1)
            vert.uv.x = static_cast<float>(j) / static_cast<float>(slices);
            vert.uv.y = static_cast<float>(i) / static_cast<float>(stacks);

            vertices.push_back(vert);
        }
    }

    // --- Generate indices ---
    // Each row has (slices + 1) vertices due to the seam duplication for UVs
    const int32_t vertsPerRow = slices + 1;

    for (int32_t i = 0; i < stacks; ++i)
    {
        for (int32_t j = 0; j < slices; ++j)
        {
            const uint32_t topLeft     = i * vertsPerRow + j;
            const uint32_t topRight    = topLeft + 1;
            const uint32_t bottomLeft  = (i + 1) * vertsPerRow + j;
            const uint32_t bottomRight = bottomLeft + 1;

            // Skip degenerate triangles at poles (optional optimization)
            if (i != 0)
            {
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
            }

            if (i != stacks - 1)
            {
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
            }
    }
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