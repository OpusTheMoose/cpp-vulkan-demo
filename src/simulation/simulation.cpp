module;
#include <array>
#include <cmath>
#include <span>
#include<glm/glm.hpp>
export module simulation;
import types;

constexpr u32 num_of_objects = 2;

constexpr double big_G = 6.674e-11;


struct ObjectData {
    std::array<glm::dvec3, num_of_objects> positions;
    std::array<glm::dvec3, num_of_objects> velocites;
    std::array<double, num_of_objects> masses;
};
export class GravitySimulation {
    public:
        GravitySimulation(){
            objects.positions[0] = glm::dvec3{0.0, 0.0, 0.0};
            objects.positions[1] = glm::vec3{0.0, 0.0, 5.0};

            objects.velocites[0] = glm::dvec3(0.0, 0.0, 0.0);
            objects.velocites[1] = glm::dvec3{0.0, 0.0, 0.0};

            objects.masses[0] = 100;
            objects.masses[1] = 10;


        }

        auto update(float dt) -> void {
         
           for (size_t pos1 = 0; pos1 < num_of_objects; pos1++)
           {
                glm::dvec3 accel_gravity = glm::dvec3{0.0};
                for (size_t pos2 = 0; pos2 < num_of_objects; pos2++ )
                {
                    if (pos2 != pos1)
                    {
                        auto r_vector = objects.positions[pos1] - objects.positions[pos2];
                        auto r_mag = glm::length(r_vector);
                        double accel = -1.0 * big_G * (objects.masses[pos2]) / pow(r_mag, 2.0);
                        auto r_unit_vector = r_vector / r_mag;
                        accel_gravity = accel * r_unit_vector;

                    }
                }
                objects.velocites[pos1] += accel_gravity * static_cast<double>(dt);
           } 
           for (size_t i = 0; i < num_of_objects; i++)
           {
                objects.positions[i] += objects.velocites[i];
           }
        }

        auto getObjectPositions() -> std::span<glm::dvec3> {
            return objects.positions;
        }

    private:
        ObjectData objects;

};