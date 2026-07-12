#pragma once

#include <dod_core/assert.hpp>
#include <dod_core/system_traits.hpp>
#include <dod_core/world.hpp>
#include <functional>
#include <string>
#include <utility>

namespace dod
{

class System
{
  public:
    template <typename Fn> System(std::string name, Fn&& fn) : m_name{std::move(name)}
    {
        using traits = SystemTraits<Fn>;
        using args = typename traits::args_tuple;

        register_access<args>(m_access, std::make_index_sequence<traits::arity>{});
        DOD_ASSERT(!has_aliased_write(m_access),
                   "System: a component written by one parameter is accessed again by another "
                   "parameter of the same system (unsynchronized aliasing)");

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
        (detail::query_kind<std::tuple_element_t<Is, Tuple>>::register_access(access), ...);
    }

    // Within one system nothing orders parameter accesses relative to each
    // other, so the same component must not be written by one parameter and
    // touched by another. Duplicate reads are harmless and stay legal.
    static bool has_aliased_write(const ResourceAccess& access)
    {
        for (std::size_t i = 0; i < access.writes.size(); ++i)
        {
            for (std::size_t j = i + 1; j < access.writes.size(); ++j)
            {
                if (access.writes[i] == access.writes[j])
                {
                    return true;
                }
            }
            for (auto read : access.reads)
            {
                if (access.writes[i] == read)
                {
                    return true;
                }
            }
        }
        return false;
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
