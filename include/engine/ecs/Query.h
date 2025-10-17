#ifndef QUERY_H
#define QUERY_H

#include "ecs/World.h"
#include <type_traits>

namespace Grape::ECS {
    template<typename... TInclude>
    class Query {
    public:
        explicit Query(World& world) : m_world(world), m_includeSig({ TypeIdOf<std::decay_t<TInclude>>()... }) {}

        template<typename TFn>
        void Each(TFn&& fn) {
            m_world.Each<TInclude...>(std::forward<TFn>(fn));
        }

    private:
        World& m_world;
        Signature m_includeSig;
    };
}

#endif
