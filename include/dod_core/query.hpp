#pragma once

#include <dod_core/entity.hpp>
#include <dod_core/world.hpp>
#include <utility>

namespace dod
{

template <typename T> class Read
{
  public:
    explicit Read(const World& world) : m_view{world.view<T>()} {}

    template <typename Fn> void each(Fn&& fn) const { m_view.each(std::forward<Fn>(fn)); }

    [[nodiscard]] const T& get(Entity entity) const
    {
        return m_view.template get<const T>(static_cast<entt::entity>(entity));
    }

    [[nodiscard]] auto begin() const { return m_view.begin(); }
    [[nodiscard]] auto end() const { return m_view.end(); }
    [[nodiscard]] bool contains(Entity entity) const
    {
        return m_view.contains(static_cast<entt::entity>(entity));
    }

  private:
    using view_type = decltype(std::declval<const World&>().view<T>());
    view_type m_view;
};

template <typename T> class Write
{
  public:
    explicit Write(World& world) : m_view{world.view<T>()} {}

    template <typename Fn> void each(Fn&& fn) { m_view.each(std::forward<Fn>(fn)); }

    [[nodiscard]] T& get(Entity entity)
    {
        return m_view.template get<T>(static_cast<entt::entity>(entity));
    }

    [[nodiscard]] auto begin() const { return m_view.begin(); }
    [[nodiscard]] auto end() const { return m_view.end(); }
    [[nodiscard]] bool contains(Entity entity) const
    {
        return m_view.contains(static_cast<entt::entity>(entity));
    }

  private:
    using view_type = decltype(std::declval<World&>().view<T>());
    view_type m_view;
};

class WorldRead
{
  public:
    explicit WorldRead(const World& world) noexcept : m_world{&world} {}

    [[nodiscard]] const World& world() const noexcept { return *m_world; }
    [[nodiscard]] const World* operator->() const noexcept { return m_world; }
    [[nodiscard]] const World& operator*() const noexcept { return *m_world; }

  private:
    const World* m_world;
};

class WorldWrite
{
  public:
    explicit WorldWrite(World& world) noexcept : m_world{&world} {}

    [[nodiscard]] World& world() noexcept { return *m_world; }
    [[nodiscard]] const World& world() const noexcept { return *m_world; }
    [[nodiscard]] World* operator->() noexcept { return m_world; }
    [[nodiscard]] const World* operator->() const noexcept { return m_world; }
    [[nodiscard]] World& operator*() noexcept { return *m_world; }
    [[nodiscard]] const World& operator*() const noexcept { return *m_world; }

  private:
    World* m_world;
};

} // namespace dod
