#pragma once

#include <dod_core/system_traits.hpp>
#include <dod_core/world.hpp>
#include <entt/core/type_info.hpp>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace dod
{

struct ResourceAccess
{
    std::vector<entt::id_type> reads;
    std::vector<entt::id_type> writes;
    bool world_read = false;
    bool world_write = false;
};

class System
{
  public:
    template <typename Fn> System(std::string name, Fn&& fn) : m_name{std::move(name)}
    {
        using traits = SystemTraits<Fn>;
        using args = typename traits::args_tuple;

        register_access<args>(m_access, std::make_index_sequence<traits::arity>{});

        m_prepare = &prepare_with_args<args>;

        m_invoker = [fn = std::forward<Fn>(fn)](World& world) mutable
        { invoke_with_args<args>(fn, world, std::make_index_sequence<traits::arity>{}); };
    }

    void operator()(World& world) const { m_invoker(world); }

    // Create every component pool this system's queries touch. EnTT creates
    // pools lazily on first (mutable) view construction, and that creation is
    // not thread-safe; the scheduler calls prepare() for all systems on the
    // dispatching thread before any of them runs on a worker, so view
    // construction inside workers only ever looks up existing pools.
    void prepare(World& world) const { m_prepare(world); }

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] const ResourceAccess& access() const noexcept { return m_access; }

  private:
    template <typename Tuple, std::size_t... Is>
    static void register_access(ResourceAccess& access, std::index_sequence<Is...>)
    {
        (register_one<std::tuple_element_t<Is, Tuple>>(access), ...);
    }

    template <typename Param> static void register_one(ResourceAccess& access)
    {
        using kind = detail::query_kind<Param>;
        if constexpr (kind::is_read)
        {
            access.reads.emplace_back(entt::type_hash<typename kind::component>::value());
        }
        else if constexpr (kind::is_write)
        {
            access.writes.emplace_back(entt::type_hash<typename kind::component>::value());
        }
        else if constexpr (kind::is_world_read)
        {
            access.world_read = true;
        }
        else if constexpr (kind::is_world_write)
        {
            access.world_write = true;
        }
    }

    template <typename Tuple, typename Fn, std::size_t... Is>
    static void invoke_with_args(Fn& fn, World& world, std::index_sequence<Is...>)
    {
        fn(detail::query_kind<std::tuple_element_t<Is, Tuple>>::construct(world)...);
    }

    template <typename Tuple> static void prepare_with_args(World& world)
    {
        prepare_impl<Tuple>(world, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
    }

    template <typename Tuple, std::size_t... Is>
    static void prepare_impl(World& world, std::index_sequence<Is...>)
    {
        (detail::query_kind<std::tuple_element_t<Is, Tuple>>::prepare(world), ...);
    }

    std::string m_name;
    ResourceAccess m_access;
    void (*m_prepare)(World&) = nullptr;
    std::function<void(World&)> m_invoker;
};

} // namespace dod
