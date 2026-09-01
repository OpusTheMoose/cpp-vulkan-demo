module;
#include <vector>
export module mesh;
import vertex;

export class Mesh {
    public:
        Mesh() {
        vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
        };
    }
        std::vector<Vertex> getVertices() {
            return vertices;
        }
                
        

    private:
        std::vector<Vertex> vertices;
};