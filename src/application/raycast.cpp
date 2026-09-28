module;
#include <glm/glm.hpp>
export module raycast;
import types;

export struct Ray {
    glm::dvec3 start;
    glm::dvec3 end;
    glm::dvec3 raydir;
    Ray(glm::dvec3 start_, glm::dvec3 end_) {
        start = start_;
        end = end_;
        raydir = glm::normalize(end - start);
    }
};
    


export class Raycast {
    public:
    Raycast() {};

    auto testRayOnSphere(Ray ray, glm::dvec3 spherePosition, float radius )
    {
        auto const distance_from_sphere = ray.start - spherePosition;
          return 
                pow(glm::dot(ray.raydir, distance_from_sphere), 2.0) -
                (pow(glm::length(distance_from_sphere), 2.0) - radius);
    }
    private:


};