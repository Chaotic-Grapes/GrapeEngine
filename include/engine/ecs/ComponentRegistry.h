#ifndef COMPONENTREGISTRY_H
#define COMPONENTREGISTRY_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <mutex>

namespace ECS {
  using ComponentTypeId = uint32_t;

  struct ComponentMeta {
      size_t Size{0};
      size_t Align{0};
      void (*ctor)(void*){nullptr};
      void (*dtor)(void*){nullptr};
  };

  class ComponentRegistry {
  public:
      template<typename T>
      static ComponentTypeId Type() {
          static const ComponentTypeId id = _nextId();
          // lazily register metadata for T
          (void)_detailRegister<T>(id);
          return id;
      }

      static const ComponentMeta& Meta(ComponentTypeId id) {
          std::lock_guard<std::mutex> lock(_mutex());
          return _metas()[id];
      }

  private:
      template<typename T>
      static bool _detailRegister(ComponentTypeId id) {
          std::lock_guard<std::mutex> lock(mutex());
          auto& m = metas()[id];
          if (m.Size == 0) {
              m.Size = sizeof(T);
              m.Align = alignof(T);
              if constexpr (std::is_trivially_constructible_v<T>) {
                  m.ctor = [](void* p) { 
                    /* do nothing for trivially constructible */
                    new (p) T();
                };
              }
              else {
                  m.ctor = [](void* p) { new (p) T(); };
              }
              if constexpr (std::is_trivially_destructible_v<T>) {
                  m.dtor = [](void*) { /* do nothing for trivially destructible */ };
              } 
              else {
                  m.dtor = [](void* p) { reinterpret_cast<T*>(p)->~T(); };
              }
          }
          return true;
      }

      static ComponentTypeId _nextId() {
          std::lock_guard<std::mutex> lock(_mutex());
          return ++_counter();
      }

      static std::unordered_map<ComponentTypeId, ComponentMeta>& _metas() {
          static std::unordered_map<ComponentTypeId, ComponentMeta> m;
          return m;
      }
      static ComponentTypeId& _counter() {
          static ComponentTypeId c = 0;
          return c;
      }
      static std::mutex& _mutex() {
          static std::mutex mtx;
          return mtx;
      }
  };

  template<typename T>
  inline ComponentTypeId type_id() { return ComponentRegistry::Type<T>(); }

  inline const ComponentMeta& component_meta(ComponentTypeId id) { return ComponentRegistry::Meta(id); }
}

#endif
