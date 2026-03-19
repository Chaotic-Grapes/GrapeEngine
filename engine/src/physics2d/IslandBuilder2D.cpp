/* Start Header *****************************************************************/
/*!
\file   IslandBuilder2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Deterministic contact-graph island construction.
\references
- https://box2d.org/documentation/
- https://en.wikipedia.org/wiki/Breadth-first_search
- https://www.geeksforgeeks.org/connected-components-in-an-undirected-graph/
*/
/* End Header *******************************************************************/

#include "physics2d/IslandBuilder2D.h"
#include <algorithm>
#include <queue>
#include <unordered_map>

namespace Engine::Physics2D {
    /**
     * @brief Build deterministic connected contact islands.
     */
    std::vector<PhysicsIsland2D> IslandBuilder2D::Build(
        const std::vector<BodyRuntime2D>& bodies,
        const std::vector<ContactConstraint2D>& contacts) const
    {
        // Graph representation: body nodes, edges are non-trigger contacts.
        std::vector<std::vector<size_t>> adjacency(bodies.size());
        std::vector<std::vector<size_t>> contactRefs(bodies.size());

        for (size_t i = 0; i < contacts.size(); ++i) {
            const auto& c = contacts[i];
            if (c.IsTrigger) {
                // Trigger-only contacts do not participate in solver islands.
                continue;
            }
            adjacency[c.BodyA].push_back(c.BodyB);
            adjacency[c.BodyB].push_back(c.BodyA);
            contactRefs[c.BodyA].push_back(i);
            contactRefs[c.BodyB].push_back(i);
        }

        std::vector<size_t> order;
        order.reserve(bodies.size());
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].IsDynamic) {
                // Start BFS only from dynamic bodies; static-only components are pulled in via edges.
                order.push_back(i);
            }
        }
        std::sort(order.begin(), order.end(), [&bodies](size_t a, size_t b) {
            return bodies[a].Packed < bodies[b].Packed;
            });

        std::vector<bool> visited(bodies.size(), false);
        std::vector<PhysicsIsland2D> islands;
        for (size_t root : order) {
            if (visited[root]) {
                continue;
            }

            PhysicsIsland2D island;
            std::queue<size_t> q;
            q.push(root);
            visited[root] = true;
            while (!q.empty()) {
                // Standard BFS gathers one connected component.
                const size_t current = q.front();
                q.pop();
                island.Bodies.push_back(current);
                for (size_t cidx : contactRefs[current]) {
                    island.Contacts.push_back(cidx);
                }

                auto neighbors = adjacency[current];
                // Sort neighbors by packed ID before BFS expansion for deterministic island order.
                std::sort(neighbors.begin(), neighbors.end(), [&bodies](size_t a, size_t b) {
                    return bodies[a].Packed < bodies[b].Packed;
                    });
                for (size_t n : neighbors) {
                    if (!visited[n]) {
                        visited[n] = true;
                        q.push(n);
                    }
                }
            }

            std::sort(island.Bodies.begin(), island.Bodies.end(), [&bodies](size_t a, size_t b) {
                return bodies[a].Packed < bodies[b].Packed;
                });
            std::sort(island.Contacts.begin(), island.Contacts.end());
            // Same contact can be reached from both endpoint bodies; deduplicate indices.
            island.Contacts.erase(std::unique(island.Contacts.begin(), island.Contacts.end()), island.Contacts.end());
            islands.push_back(std::move(island));
        }

        std::sort(islands.begin(), islands.end(), [&bodies](const PhysicsIsland2D& a, const PhysicsIsland2D& b) {
            if (a.Bodies.empty() || b.Bodies.empty()) return a.Bodies.size() < b.Bodies.size();
            return bodies[a.Bodies.front()].Packed < bodies[b.Bodies.front()].Packed;
            });
        return islands;
    }
}
