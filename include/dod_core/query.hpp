#pragma once

#include <dod_core/entity.hpp>
#include <dod_core/world.hpp>
#include <type_traits>
#include <utility>

namespace dod
{

namespace detail
{

// True when no component type appears twice in the pack; const and non-const
// count as the same component.
template <typename... Ts> struct components_unique : std::true_type
{
};

template <typename T, typename... Rest>
struct components_unique<T, Rest...>
    : std::bool_constant<
          (!std::is_same_v<std::remove_const_t<T>, std::remove_const_t<Rest>> && ...) &&
          components_unique<Rest...>::value>
{
};

} // namespace detail

// Joint view over entities that have ALL listed components. Access mode is
// inferred per component from const qualification (EnTT's own convention):
// `View<Position, const Velocity>` writes Position and reads Velocity.
template <typename... Ts> class View
{
    static_assert(sizeof...(Ts) > 0, "View requires at least one component type");
    static_assert(detail::components_unique<Ts...>::value,
                  "View: duplicate component type (const and non-const are the same component)");

  public:
    explicit View(World& world) : m_view{world.view<Ts...>()} {}

    // Callback takes (Entity, components...) or (components...); each
    // component parameter is `T&` if declared non-const, `const T&` if const.
    template <typename Fn> void each(Fn&& fn) const { m_view.each(std::forward<Fn>(fn)); }

    // Const qualification must match the declaration: a View<Position,
    // const Velocity> serves get<Position> and get<const Velocity>.
    template <typename... Cs> [[nodiscard]] decltype(auto) get(Entity entity) const
    {
        return m_view.template get<Cs...>(static_cast<entt::entity>(entity));
    }

    // Single-component sugar: `view.get(e)` instead of `view.get<T>(e)`.
    [[nodiscard]] decltype(auto) get(Entity entity) const
        requires(sizeof...(Ts) == 1)
    {
        return m_view.template get<Ts...>(static_cast<entt::entity>(entity));
    }

    [[nodiscard]] auto begin() const { return m_view.begin(); }
    [[nodiscard]] auto end() const { return m_view.end(); }
    [[nodiscard]] bool contains(Entity entity) const
    {
        return m_view.contains(static_cast<entt::entity>(entity));
    }

  private:
    using view_type = decltype(std::declval<World&>().view<Ts...>());
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
