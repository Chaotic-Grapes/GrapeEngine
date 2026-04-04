/* Start Header *****************************************************************/
/*!
\file    Signature.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Signature class,
responsible for managing component signatures in the Entity-Component-System (ECS)
architecture. It provides methods for checking component presence, merging,
and removing components from signatures.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef SIGNATURE_H
#define SIGNATURE_H

#include <vector>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "ecs/ComponentRegistry.h"
#include "ecs/Entity.h"

namespace ECS {
    // Signature represents a set of component types using their TypeIds.
    class Signature {
    public:
        Signature() { m_types.reserve(8); }  // Reserve space for common case
        explicit Signature(std::vector<TypeId> types) : m_types(std::move(types)) {
            std::sort(m_types.begin(), m_types.end());
            m_types.erase(std::unique(m_types.begin(), m_types.end()), m_types.end());
            m_types.shrink_to_fit();  // Free unused memory after deduplication
        }

        /**
         * @brief Checks if the signature contains the specified component type.
         * @param t The TypeId of the component to check for.
         * @return True if the component type is present in the signature, false otherwise.
         */
        bool Contains(const TypeId t) const {
            return std::binary_search(m_types.begin(), m_types.end(), t);
        }

        /**
         * @brief Checks if the signature contains any of the component types in the given signature.
         * @param req The signature containing component types to check for.
         * @return True if any component type from req is present in this signature, false otherwise.
         */
        bool ContainsAny(const Signature& req) const {
            for (auto rt : req.m_types) {
                if (Contains(rt)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Checks if the signature contains all of the component types in the given signature.
         * @param req The signature containing component types to check for.
         * @return True if all component types from req are present in this signature, false otherwise.
         */
        bool ContainsAll(const Signature& req) const {
            auto it = m_types.begin();

            for (auto rt : req.m_types) {
                it = std::lower_bound(it, m_types.end(), rt);

                if (it == m_types.end() || *it != rt)
                    return false;

                ++it;
            }

            return true;
        }

        /**
         * @brief Creates a new signature by merging this signature with the specified component type.
         * @param t The TypeId of the component to add.
         * @return A new Signature that includes the specified component type.
         */
        Signature MergedWith(const TypeId t) const {
            Signature out = *this;

            out.m_types.insert(std::lower_bound(out.m_types.begin(), out.m_types.end(), t), t);
            out.m_types.erase(std::unique(out.m_types.begin(), out.m_types.end()), out.m_types.end());

            return out;
        }

        /**
         * @brief Creates a new signature by removing the specified component type from this signature.
         * @param t The TypeId of the component to remove.
         * @return A new Signature that excludes the specified component type.
         */
        Signature Without(const TypeId t) const {
            Signature out = *this;

            const auto it = std::lower_bound(out.m_types.begin(), out.m_types.end(), t);
            if (it != out.m_types.end() && *it == t)
                out.m_types.erase(it);

            return out;
        }

        /**
         * @brief Test whether two signatures contain the same component types.
         * @param o The signature to compare against.
         * @return True if both signatures contain identical component types.
         */
        bool operator==(const Signature& o) const { return m_types == o.m_types; }

        /**
         * @brief Test whether two signatures differ in component types.
         * @param o The signature to compare against.
         * @return True if the signatures contain different component types.
         */
        bool operator!=(const Signature& o) const { return !(*this == o); }

        /**
         * @brief Gets the list of component types in this signature.
         * @return A const reference to the vector of TypeIds.
         */
        const std::vector<TypeId>& Types() const  { return m_types; }

    private:
        std::vector<TypeId> m_types;
    };

    struct SignatureHash {
        // Hashes a Signature for use in unordered containers.
        // Uses FNV1a hash algorithm.
        size_t operator()(const Signature& s) const noexcept {
            // FNV1a 64-bit
            // FNV1a hash algorithm is simple and fast for small data sets
            // http://www.isthe.com/chongo/tech/comp/fnv/ to read more
            // hash = offset_basis ^ (key * prime)
            size_t h = 1469598103934665603ull;
            for (const auto t : s.Types()) {
                h ^= static_cast<size_t>(t);
                h *= 1099511628211ull;
            }
            return h;
        }
    };
}

#endif
