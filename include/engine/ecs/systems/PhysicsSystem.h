#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include "ecs/World.h"
#include <vector>
#include <unordered_map>

namespace ECS {
    class PhysicsSystem {
    public:
        static void Update(World& world, float dt);
    private: 
    //    struct SpatialGrid {
    //        std::unordered_map<int, std::vector<size_t>> cells;

    //        //clears all entities from grid
    //        void Clear() {
    //            //loop through all cells and clear their entity lists
    //            for (auto& [key, vec] : cells) {
    //                vec.clear();
    //            }
    //        }
    //        //inserts entity into spatial grid
    //        void Insert(size_t index, float x, float y) {
    //            //calculate which cells this position falls into 
    //            // cell size = 100 units (adjustable if entities are larger/smaller)
    //            int cellX = static_cast<int>(x / 100.f);
    //            int cellY = static_cast<int>(y / 100.f);

    //            // pack 2D cell cords into a single integer key
    //            // allows us to use hash map for 0(1) lookups
    //            // format: (cellY <<16) | cellX
    //            int key = (cellY << 16) | (cellX & 0xFFFF);

    //            //add this entity's index to the cell
    //            cells[key].push_back(index);
    //        }

    //        std::vector<size_t> GetNearby(float x, float y) const {
    //            std::vector<size_t> result;

    //            // Calculate which cell this position is in
    //            int cellX = static_cast<int>(x / 100.0f);
    //            int cellY = static_cast<int>(y / 100.0f);

    //            //Check the 3x3 grid of cells (center cell + 8 neighbors)
    //            for (int dy = -1; dy <= 1; ++dy) {        // -1, 0, +1 (rows)
    //                for (int dx = -1; dx <= 1; ++dx) {    // -1, 0, +1 (columns)
    //                    // Calculate neighbor cell's key
    //                    int key = ((cellY + dy) << 16) | ((cellX + dx) & 0xFFFF);

    //                    // Check if this cell exists in our map
    //                    auto it = cells.find(key);
    //                    if (it != cells.end()) {
    //                        // Add all entity indices from this cell to our result
    //                        result.insert(result.end(), it->second.begin(), it->second.end());
    //                    }
    //                }
    //            }

    //            return result;
    //        }
    //};
    //    //Static instance of the spatial grid (shared across all Update calls)
    //    static SpatialGrid s_spatialGrid;
    };
}

#endif