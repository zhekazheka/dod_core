#pragma once

#include <dod_core/entity.hpp>
#include <entt/entity/registry.hpp>
#include <utility>

namespace dod
{

class World
{
  public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    // ── Entity lifecycle ────────────────────────────────────
    [[nodiscard]] Entity create() { return Entity{m_registry.create()}; }

    void destroy(Entity entity) { m_registry.destroy(entity); }

    [[nodiscard]] bool alive(Entity entity) const { return m_registry.valid(entity); }

    // ── Component operations ────────────────────────────────
    template <typename T, typename... Args> decltype(auto) emplace(Entity entity, Args&&... args)
    {
        return m_registry.emplace<T>(static_cast<entt::entity>(entity), std::forward<Args>(args)...);
    }

    template <typename T> void remove(Entity entity)
    {
        m_registry.remove<T>(static_cast<entt::entity>(entity));
    }

    template <typename T> [[nodiscard]] T& get(Entity entity)
    {
        return m_registry.get<T>(static_cast<entt::entity>(entity));
    }

    template <typename T> [[nodiscard]] const T& get(Entity entity) const
    {
        return m_registry.get<T>(static_cast<entt::entity>(entity));
    }

    template <typename... Ts> [[nodiscard]] bool has(Entity entity) const
    {
        return m_registry.all_of<Ts...>(static_cast<entt::entity>(entity));
    }

    // ── Queries ─────────────────────────────────────────────
    template <typename... Ts> [[nodiscard]] auto view() { return m_registry.view<Ts...>(); }

    template <typename... Ts> [[nodiscard]] auto view() const
    {
        return m_registry.view<std::add_const_t<Ts>...>();
    }

    // ── Escape hatch ────────────────────────────────────────
    [[nodiscard]] entt::registry& registry() noexcept { return m_registry; }
    [[nodiscard]] const entt::registry& registry() const noexcept { return m_registry; }

  private:
    entt::registry m_registry;
};

} // namespace dod
