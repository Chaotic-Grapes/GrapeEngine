/* Start Header *****************************************************************/
/*!
\file   Broadphase2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Deterministic dynamic AABB-tree broadphase implementation.
\references
- https://box2d.org/posts/2014/08/balancing-dynamic-trees/
- https://box2d.org/documentation/
- https://realtimecollisiondetection.net/books/rtcd/
- https://allenchou.net/2014/02/game-physics-broadphase-dynamic-aabb-tree/
*/
/* End Header *******************************************************************/

#include "physics2d/Broadphase2D.h"
#include "physics2d/internal/ParallelFor.h"
#include "physics2d/internal/SimdKernels2D.h"
#include <algorithm>
#include <cmath>

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
     * @brief Test whether `outer` fully contains `inner`.
     * @param outer Candidate containing AABB.
     * @param inner Candidate contained AABB.
     * @return True when `inner` is fully enclosed by `outer`.
     */
    bool Broadphase2D::Contains(const Engine::WorldAABB& outer, const Engine::WorldAABB& inner) {
        const Vector2D outerMin = outer.Center - outer.HalfExtents;
        const Vector2D outerMax = outer.Center + outer.HalfExtents;
        const Vector2D innerMin = inner.Center - inner.HalfExtents;
        const Vector2D innerMax = inner.Center + inner.HalfExtents;
        return innerMin.X >= outerMin.X && innerMin.Y >= outerMin.Y &&
            innerMax.X <= outerMax.X && innerMax.Y <= outerMax.Y;
    }

    /**
     * @brief Expand an AABB by configured fat padding.
     * @param source Source AABB.
     * @param fatPadding Extra half-extent on each axis.
     * @return Expanded AABB.
     */
    Engine::WorldAABB Broadphase2D::ExpandFat(const Engine::WorldAABB& source, float fatPadding) {
        Engine::WorldAABB out = source;
        out.HalfExtents.X += fatPadding;
        out.HalfExtents.Y += fatPadding;
        return out;
    }

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
        // Create new leaf node that stores one runtime body index.
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
            // Descend toward child that causes smaller perimeter growth.
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
        // Insert a new internal parent above sibling + new leaf.
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
                // Recompute internal AABB from both children.
                m_nodes[index].Box = Merge(m_nodes[m_nodes[index].Left].Box, m_nodes[m_nodes[index].Right].Box);
            }
            index = m_nodes[index].Parent;
        }
    }

    /**
     * @brief Refit internal-node bounds from a node's parent up to root.
     * @param nodeIndex First node to refit.
     * @return None.
     */
    void Broadphase2D::RefitAncestors(int nodeIndex) {
        int current = nodeIndex;
        while (current != -1) {
            if (!m_nodes[current].IsLeaf) {
                m_nodes[current].Box = Merge(m_nodes[m_nodes[current].Left].Box, m_nodes[m_nodes[current].Right].Box);
            }
            current = m_nodes[current].Parent;
        }
    }

    /**
     * @brief Check whether incremental updates should fallback to full rebuild.
     * @param bodies Runtime body cache.
     * @param fatPadding Current fat AABB padding.
     * @return True when rebuild is required.
     */
    bool Broadphase2D::RequiresRebuild(const std::vector<BodyRuntime2D>& bodies, float fatPadding) {
        if (m_root == -1 || m_bindings.empty()) {
            return true;
        }
        if (std::abs(m_fatPadding - fatPadding) > 1e-6f) {
            return true;
        }
        if (++m_stepsSinceRebuild > 240u) {
            return true;
        }

        std::vector<PackedEntityId> packed;
        packed.reserve(bodies.size());
        for (const auto& body : bodies) {
            if (body.Shape != ShapeType2D::None) {
                packed.push_back(body.Packed);
            }
        }
        // Any body-set mismatch means bindings/tree topology is stale.
        if (packed.size() != m_bindings.size()) {
            return true;
        }
        std::sort(packed.begin(), packed.end());
        for (size_t i = 0; i < packed.size(); ++i) {
            if (packed[i] != m_bindings[i].Packed) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Rebuild broadphase tree from body cache.
     */
    void Broadphase2D::Build(const std::vector<BodyRuntime2D>& bodies, float fatPadding) {
        // Full rebuild resets tree + proxy mapping from scratch.
        m_nodes.clear();
        m_bindings.clear();
        m_bindingByPacked.clear();
        m_root = -1;
        m_fatPadding = fatPadding;
        m_stepsSinceRebuild = 0;

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
            const auto fat = ExpandFat(bodies[idx].WorldAabb, fatPadding);
            const int nodeIndex = static_cast<int>(m_nodes.size());
            InsertLeaf(idx, fat);
            // Keep packed-id -> binding index mapping for fast incremental updates.
            ProxyBinding binding{};
            binding.Packed = bodies[idx].Packed;
            binding.BodyIndex = idx;
            binding.NodeIndex = nodeIndex;
            m_bindingByPacked[binding.Packed] = m_bindings.size();
            m_bindings.push_back(binding);
        }
    }

    /**
     * @brief Incrementally update broadphase proxy bindings and leaf bounds.
     * @param bodies Runtime body cache.
     * @param fatPadding Extra half-extent padding for leaf AABBs.
     * @return None.
     */
    void Broadphase2D::Update(const std::vector<BodyRuntime2D>& bodies, float fatPadding) {
        if (RequiresRebuild(bodies, fatPadding)) {
            Build(bodies, fatPadding);
            return;
        }

        // Build quick packed-id -> body-index map for this frame's runtime body cache.
        std::unordered_map<PackedEntityId, size_t> bodyByPacked;
        bodyByPacked.reserve(m_bindings.size());
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].Shape == ShapeType2D::None) {
                continue;
            }
            bodyByPacked[bodies[i].Packed] = i;
        }

        for (auto& binding : m_bindings) {
            const auto it = bodyByPacked.find(binding.Packed);
            if (it == bodyByPacked.end()) {
                Build(bodies, fatPadding);
                return;
            }

            const size_t bodyIndex = it->second;
            binding.BodyIndex = bodyIndex;
            TreeNode& node = m_nodes[binding.NodeIndex];
            node.BodyIndex = bodyIndex;

            const Engine::WorldAABB currentFat = ExpandFat(bodies[bodyIndex].WorldAabb, fatPadding);
            if (Contains(node.Box, currentFat)) {
                // Existing fat box still valid; no tree mutation required.
                continue;
            }

            // Expand leaf bounds and refit ancestors. A periodic full rebuild prevents unbounded fat growth.
            node.Box = Merge(node.Box, currentFat);
            RefitAncestors(node.Parent);
        }
    }

    /**
     * @brief Query tree for leaves overlapping the given AABB.
     */
    void Broadphase2D::QueryOverlaps(const Engine::WorldAABB& query, std::vector<size_t>& outBodies) const {
        if (m_root == -1) {
            return;
        }
        // Iterative DFS avoids recursion and keeps traversal deterministic.
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
                // Push both children for continued overlap traversal.
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
                // Query all bodies with AABB overlap against body A.
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
                        // Static-static pairs cannot generate solver constraints.
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
