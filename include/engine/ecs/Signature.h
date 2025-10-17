#ifndef SIGNATURE_H
#define SIGNATURE_H

#include <vector>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "ecs/ComponentRegistry.h"

namespace ECS {
    class Signature {
    public:
        Signature() = default;
        explicit Signature(std::vector<TypeId> types) : m_types(std::move(types)) {
            std::sort(m_types.begin(), m_types.end());
            m_types.erase(std::unique(m_types.begin(), m_types.end()), m_types.end());
        }

        bool Contains(TypeId t) const {
            return std::binary_search(m_types.begin(), m_types.end(), t);
        }
        bool ContainsAll(const Signature& req) const {
            auto it = m_types.begin();
            for (auto rt : req.m_types) {
                it = std::lower_bound(it, m_types.end(), rt);
                if (it == m_types.end() || *it != rt) return false;
                ++it;
            }
            return true;
        }
        Signature MergedWith(TypeId t) const {
            Signature out = *this;
            out.m_types.insert(std::lower_bound(out.m_types.begin(), out.m_types.end(), t), t);
            out.m_types.erase(std::unique(out.m_types.begin(), out.m_types.end()), out.m_types.end());
            return out;
        }
        Signature Without(TypeId t) const {
            Signature out = *this;
            auto it = std::lower_bound(out.m_types.begin(), out.m_types.end(), t);
            if (it != out.m_types.end() && *it == t) out.m_types.erase(it);
            return out;
        }

        bool operator==(const Signature& o) const { return m_types == o.m_types; }
        bool operator!=(const Signature& o) const { return !(*this == o); }

        const std::vector<TypeId>& Types() const { return m_types; }

    private:
        std::vector<TypeId> m_types;
    };

    struct SignatureHash {
        size_t operator()(const Signature& s) const noexcept {
            // FNV1a 64-bit
            // FNV1a hash algorithm is simple and fast for small data sets
            // http://www.isthe.com/chongo/tech/comp/fnv/ to read more
            // hash = offset_basis ^ (key * prime)
            size_t h = 1469598103934665603ull;
            for (auto t : s.Types()) {
                h ^= static_cast<size_t>(t);
                h *= 1099511628211ull;
            }
            return h;
        }
    };
}

#endif
