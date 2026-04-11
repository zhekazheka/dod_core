#pragma once

#include <compare>
#include <cstdint>
#include <entt/entity/entity.hpp>
#include <type_traits>

namespace dod
{

class Entity
{
  public:
    constexpr Entity() noexcept : m_handle{entt::null} {}
    constexpr Entity(entt::entity handle) noexcept : m_handle{handle} {}

    [[nodiscard]] constexpr bool valid() const noexcept { return m_handle != entt::null; }

    [[nodiscard]] constexpr auto raw() const noexcept -> std::uint32_t
    {
        return static_cast<std::uint32_t>(entt::to_integral(m_handle));
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] constexpr operator entt::entity() const noexcept { return m_handle; }

    [[nodiscard]] constexpr auto operator==(const Entity& other) const noexcept -> bool = default;

    [[nodiscard]] constexpr auto operator<=>(const Entity& other) const noexcept
    {
        return entt::to_integral(m_handle) <=> entt::to_integral(other.m_handle);
    }

    static constexpr Entity null() noexcept { return Entity{}; }

  private:
    entt::entity m_handle;
};

static_assert(sizeof(Entity) == sizeof(entt::entity));
static_assert(std::is_trivially_copyable_v<Entity>);

} // namespace dod
