/* Start Header *****************************************************************/
/*!
\file   Broadphase2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Deterministic dynamic AABB-tree broadphase implementation.
*/
/* End Header *******************************************************************/

#include "physics2d/Broadphase2D.h"
#include "physics2d/internal/ParallelFor.h"
#include "physics2d/internal/SimdKernels2D.h"
#include <algorithm>

namespace Engine::Physics2D {
    /**
     * @brief Merge two AABBs into one enclosing AABB.
     */
    Engine::WorldAABB Broadphase2D::Merge(const Engine::WorldAABB& a, const Engine::WorldAABB& b) {
        const Vector2D aMin = a.Center - a.HalfExtents;
        const Vector2D aMax = a.Center + a.HalfExtents;
        const Vector2D bMin = b.Center - b.HalfExtents;
        const Vector2D bMax = b.Center + b.HalfExtents;
        const Vector2D mn(std::min(aMin.X, bMin.X), std::min(aMin.Y, bMin.Y));
        const Vector2D mx(std::max(aMax.X, bMax.X), std::max(aMax.Y, bMax.Y));
        Engine::WorldAABB out{};
        out.Center = (mn + mx) * 0.5f;
        out.HalfExtents = (mx - mn) * 0.5f;
        return out;
    }

    /**
     * @brief AABB overlap predicate routed through SIMD-capable kernels.
     */
    bool Broadphase2D::Overlap(const Engine::WorldAABB& a, const Engine::WorldAABB& b) { return Internal::OverlapAABB2D(a, b); }

    /**
     * @brief Heuristic perimeter metric used by tree insertion.
     */
    float Broadphase2D::Perimeter(const Engine::WorldAABB& a) {
        return 4.0f * (a.HalfExtents.X + a.HalfExtents.Y);
    }

    /**
     * @brief Insert a body AABB leaf into the broadphase tree.
     */
    void Broadphase2D::InsertLeaf(size_t bodyIndex, const Engine::WorldAABB& fatAabb) {
        TreeNode leaf{};
        leaf.Box = fatAabb;
        leaf.BodyIndex = bodyIndex;
        leaf.IsLeaf = true;
        const int leafIndex = static_cast<int>(m_nodes.size());
        m_nodes.push_back(leaf);

        if (m_root == -1) {
            m_root = leafIndex;
            return;
        }

        int sibling = m_root;
        while (!m_nodes[sibling].IsLeaf) {
            const int left = m_nodes[sibling].Left;
            const int right = m_nodes[sibling].Right;
            const auto mergedLeft = Merge(m_nodes[left].Box, fatAabb);
            const auto mergedRight = Merge(m_nodes[right].Box, fatAabb);
            const float costLeft = Perimeter(mergedLeft) - Perimeter(m_nodes[left].Box);
            const float costRight = Perimeter(mergedRight) - Perimeter(m_nodes[right].Box);
            sibling = (costLeft <= costRight) ? left : right;
        }

        const int oldParent = m_nodes[sibling].Parent;
        const int newParent = static_cast<int>(m_nodes.size());
        TreeNode parent{};
        parent.IsLeaf = false;
        parent.Left = sibling;
        parent.Right = leafIndex;
        parent.Parent = oldParent;
        parent.Box = Merge(m_nodes[sibling].Box, fatAabb);
        m_nodes.push_back(parent);
        m_nodes[sibling].Parent = newParent;
        m_nodes[leafIndex].Parent = newParent;

        if (oldParent == -1) {
            m_root = newParent;
        }
        else {
            if (m_nodes[oldParent].Left == sibling) {
                m_nodes[oldParent].Left = newParent;
            }
            else {
                m_nodes[oldParent].Right = newParent;
            }
        }

        int index = newParent;
        while (index != -1) {
            if (!m_nodes[index].IsLeaf) {
                m_nodes[index].Box = Merge(m_nodes[m_nodes[index].Left].Box, m_nodes[m_nodes[index].Right].Box);
            }
            index = m_nodes[index].Parent;
        }
    }

    /**
     * @brief Rebuild broadphase tree from body cache.
     */
    void Broadphase2D::Build(const std::vector<BodyRuntime2D>& bodies, float fatPadding) {
        m_nodes.clear();
        m_root = -1;

        std::vector<size_t> order;
        order.reserve(bodies.size());
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].Shape == ShapeType2D::None) {
                continue;
            }
            order.push_back(i);
        }
        std::sort(order.begin(), order.end(), [&bodies](size_t a, size_t b) {
            // Stable packed-ID ordering keeps tree shape deterministic across runs.
            return bodies[a].Packed < bodies[b].Packed;
            });

        for (size_t idx : order) {
            auto fat = bodies[idx].WorldAabb;
            fat.HalfExtents.X += fatPadding;
            fat.HalfExtents.Y += fatPadding;
            InsertLeaf(idx, fat);
        }
    }

    /**
     * @brief Query tree for leaves overlapping the given AABB.
     */
    void Broadphase2D::QueryOverlaps(const Engine::WorldAABB& query, std::vector<size_t>& outBodies) const {
        if (m_root == -1) {
            return;
        }
        std::vector<int> stack;
        stack.reserve(64);
        stack.push_back(m_root);
        while (!stack.empty()) {
            const int nodeIdx = stack.back();
            stack.pop_back();
            const TreeNode& node = m_nodes[nodeIdx];
            if (!Overlap(node.Box, query)) {
                continue;
            }
            if (node.IsLeaf) {
                outBodies.push_back(node.BodyIndex);
            }
            else {
                stack.push_back(node.Left);
                stack.push_back(node.Right);
            }
        }
    }

    /**
     * @brief Produce deterministic broadphase candidate pairs.
     */
    std::vector<BroadphasePair2D> Broadphase2D::GeneratePairsDeterministicParallel(const std::vector<BodyRuntime2D>& bodies) const {
        std::vector<size_t> sorted;
        sorted.reserve(bodies.size());
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].Shape != ShapeType2D::None) {
                sorted.push_back(i);
            }
        }
        std::sort(sorted.begin(), sorted.end(), [&bodies](size_t a, size_t b) {
            return bodies[a].Packed < bodies[b].Packed;
            });

        std::vector<std::vector<BroadphasePair2D>> perWorker;
        const uint32_t workerCap = 8;
        perWorker.resize(workerCap);
        Internal::ParallelForStatic(sorted.size(), workerCap, [this, &bodies, &sorted, &perWorker](size_t begin, size_t end, uint32_t workerIdx) {
            auto& localPairs = perWorker[workerIdx];
            std::vector<size_t> overlaps;
            overlaps.reserve(64);
            for (size_t i = begin; i < end; ++i) {
                const size_t bodyIndexA = sorted[i];
                const BodyRuntime2D& a = bodies[bodyIndexA];
                overlaps.clear();
                QueryOverlaps(a.WorldAabb, overlaps);
                for (size_t bodyIndexB : overlaps) {
                    if (bodyIndexA == bodyIndexB) {
                        continue;
                    }
                    const BodyRuntime2D& b = bodies[bodyIndexB];
                    if (a.Packed >= b.Packed) {
                        // Emit each logical pair once in canonical packed-ID order.
                        continue;
                    }
                    if (!a.IsDynamic && !b.IsDynamic) {
                        continue;
                    }
                    BroadphasePair2D pair{};
                    pair.BodyA = bodyIndexA;
                    pair.BodyB = bodyIndexB;
                    pair.PackedA = a.Packed;
                    pair.PackedB = b.Packed;
                    localPairs.push_back(pair);
                }
            }
            });
        std::vector<BroadphasePair2D> pairs;
        for (auto& local : perWorker) {
            // Merge in fixed worker-index order to preserve deterministic pair stream.
            pairs.insert(pairs.end(), local.begin(), local.end());
        }

        std::sort(pairs.begin(), pairs.end(), [](const BroadphasePair2D& lhs, const BroadphasePair2D& rhs) {
            if (lhs.PackedA != rhs.PackedA) return lhs.PackedA < rhs.PackedA;
            if (lhs.PackedB != rhs.PackedB) return lhs.PackedB < rhs.PackedB;
            if (lhs.BodyA != rhs.BodyA) return lhs.BodyA < rhs.BodyA;
            return lhs.BodyB < rhs.BodyB;
            });
        pairs.erase(std::unique(pairs.begin(), pairs.end(), [](const BroadphasePair2D& a, const BroadphasePair2D& b) {
            // Dedup by packed IDs so duplicate tree queries collapse to one pair.
            return a.PackedA == b.PackedA && a.PackedB == b.PackedB;
            }), pairs.end());
        return pairs;
    }
}
