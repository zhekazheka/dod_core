#pragma once

#include <dod_core/system_traits.hpp>
#include <dod_core/world.hpp>
#include <functional>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

namespace dod
{

struct ResourceAccess
{
    std::vector<std::type_index> reads;
    std::vector<std::type_index> writes;
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

        m_invoker = [fn = std::forward<Fn>(fn)](World& world) mutable
        { invoke_with_args<args>(fn, world, std::make_index_sequence<traits::arity>{}); };
    }

    void operator()(World& world) const { m_invoker(world); }

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
            access.reads.emplace_back(typeid(typename kind::component));
        }
        else if constexpr (kind::is_write)
        {
            access.writes.emplace_back(typeid(typename kind::component));
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

    std::string m_name;
    ResourceAccess m_access;
    std::function<void(World&)> m_invoker;
};

} // namespace dod
