module;
#include <array>
#include <chrono>
#include <cmath>
#include <common/TracySystem.hpp>
#include <mutex>
#include <tracy/Tracy.hpp>
#include <stop_token>
#include<glm/glm.hpp>
#include <thread>
export module simulation;
import types;

constexpr u32 num_of_objects = 2;

constexpr double big_G = 6.674e-11;


struct ObjectData {
    std::array<glm::dvec3, num_of_objects> positions;
    std::array<glm::dvec3, num_of_objects> velocites;
    std::array<double, num_of_objects> masses;
};
// Single version of ObjectData, just a non-owning view of a single piece of simulation data
export struct SimulationObject {
    glm::dvec3 position;
    glm::dvec3 velocity;
    double mass;
    size_t id;
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

        auto runSimulation(std::stop_token stop) {
            tracy::SetThreadName("Simulation");
            using clock = std::chrono::steady_clock;
            const auto step = std::chrono::duration_cast<clock::duration>(
                std::chrono::duration<double>(dt));

            auto next_update = clock::now();
            while(!stop.stop_requested()){
                {
                    ZoneScopedN("Simulation Update");
                    update(dt);
                }
            
                next_update += step;
         
                std::this_thread::sleep_until(next_update);
                

                if (clock::now() > next_update + step) next_update = clock::now();
            }

        }

        auto update(double dt) -> void {
            std::scoped_lock lock(objects_mutex);
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
                        accel_gravity += accel * r_unit_vector;

                    }
                }
                objects.velocites[pos1] += accel_gravity * dt * rateOfTime;
           } 
           for (size_t i = 0; i < num_of_objects; i++)
           {
           
                objects.positions[i] += objects.velocites[i] * dt * rateOfTime;
           }
        }

        auto getObjectPositions() -> std::array<glm::dvec3, num_of_objects> const {
            std::scoped_lock lock(objects_mutex);
            return objects.positions;
        }
        auto getSimulationHz() const -> double {return simulation_hz; }
        auto getSimulationObjectCount() const -> size_t {return num_of_objects; }
        auto getSimulationSpeed() const -> double {return rateOfTime; }
        auto setSimulationSpeed(double newRate) -> void {std::scoped_lock lock(objects_mutex); rateOfTime = newRate;  }


        /* id is position in the objects array. Note this requires obtaining a mutex lock,
            so use this function sparingly
        */ 
        auto getObjectFromId(size_t objectPos) {
            std::scoped_lock lock(objects_mutex);
            assert(objectPos < num_of_objects);
            return SimulationObject {
                objects.positions[objectPos],
                objects.velocites[objectPos],
                objects.masses[objectPos],
                objectPos
            };
        }


    private:
        std::mutex objects_mutex;
        double simulation_hz = 60.0;
        double dt = 1.0 / simulation_hz;
        double rateOfTime = 1.0;
        ObjectData objects;

};