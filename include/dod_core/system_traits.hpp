#pragma once

#include <cstddef>
#include <dod_core/query.hpp>
#include <entt/core/type_info.hpp>
#include <tuple>
#include <type_traits>
#include <vector>

namespace dod
{

// Runtime mirror of a system's compile-time access declaration. Component
// types are erased to their EnTT type hashes (const stripped first, so
// View<T> and View<const T> refer to the same component).
struct ResourceAccess
{
    std::vector<entt::id_type> reads;
    std::vector<entt::id_type> writes;
    bool world_read = false;
    bool world_write = false;
};

namespace detail
{

template <typename Fn>
struct function_traits : function_traits<decltype(&std::remove_cvref_t<Fn>::operator())>
{
};

template <typename R, typename... Args> struct function_traits<R(Args...)>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

// noexcept is part of the function type since C++17, so noexcept functions
// and function pointers need their own specializations.
template <typename R, typename... Args>
struct function_traits<R(Args...) noexcept> : function_traits<R(Args...)>
{
};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)>
{
};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept> : function_traits<R(Args...)>
{
};

template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...)> : function_traits<R(Args...)>
{
};

template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...) const> : function_traits<R(Args...)>
{
};

template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...) noexcept> : function_traits<R(Args...)>
{
};

template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...) const noexcept> : function_traits<R(Args...)>
{
};

// Each specialization describes one system-parameter kind: how many
// components it reads/writes, how to record them in a ResourceAccess, how to
// pre-create their pools, and how to construct the parameter at dispatch.
template <typename T> struct query_kind
{
    static_assert(sizeof(T) == 0,
                  "System parameter must be View<Ts...>, WorldRead, or WorldWrite");
};

template <typename... Ts> struct query_kind<View<Ts...>>
{
    static constexpr std::size_t read_count = (std::size_t{std::is_const_v<Ts>} + ... + 0);
    static constexpr std::size_t write_count = sizeof...(Ts) - read_count;
    static constexpr bool is_world_read = false;
    static constexpr bool is_world_write = false;

    static View<Ts...> construct(World& world) { return View<Ts...>{world}; }

    static void prepare(World& world)
    {
        (world.registry().storage<std::remove_const_t<Ts>>(), ...);
    }

    static void register_access(ResourceAccess& access)
    {
        (register_component<Ts>(access), ...);
    }

  private:
    template <typename T> static void register_component(ResourceAccess& access)
    {
        auto& target = std::is_const_v<T> ? access.reads : access.writes;
        target.push_back(entt::type_hash<std::remove_const_t<T>>::value());
    }
};

template <> struct query_kind<WorldRead>
{
    static constexpr std::size_t read_count = 0;
    static constexpr std::size_t write_count = 0;
    static constexpr bool is_world_read = true;
    static constexpr bool is_world_write = false;

    static WorldRead construct(World& world) { return WorldRead{world}; }
    static void prepare(World&) {}
    static void register_access(ResourceAccess& access) { access.world_read = true; }
};

template <> struct query_kind<WorldWrite>
{
    static constexpr std::size_t read_count = 0;
    static constexpr std::size_t write_count = 0;
    static constexpr bool is_world_read = false;
    static constexpr bool is_world_write = true;

    static WorldWrite construct(World& world) { return WorldWrite{world}; }
    static void prepare(World&) {}
    static void register_access(ResourceAccess& access) { access.world_write = true; }
};

} // namespace detail

template <typename Fn> struct SystemTraits
{
  private:
    using ft = detail::function_traits<std::remove_cvref_t<Fn>>;

  public:
    using args_tuple = typename ft::args_tuple;
    static constexpr std::size_t arity = ft::arity;

  private:
    template <std::size_t... Is>
    static constexpr std::size_t count_reads(std::index_sequence<Is...>)
    {
        return (detail::query_kind<std::tuple_element_t<Is, args_tuple>>::read_count + ... + 0);
    }

    template <std::size_t... Is>
    static constexpr std::size_t count_writes(std::index_sequence<Is...>)
    {
        return (detail::query_kind<std::tuple_element_t<Is, args_tuple>>::write_count + ... + 0);
    }

    template <std::size_t... Is> static constexpr bool any_world_read(std::index_sequence<Is...>)
    {
        return (detail::query_kind<std::tuple_element_t<Is, args_tuple>>::is_world_read || ...);
    }

    template <std::size_t... Is> static constexpr bool any_world_write(std::index_sequence<Is...>)
    {
        return (detail::query_kind<std::tuple_element_t<Is, args_tuple>>::is_world_write || ...);
    }

  public:
    static constexpr std::size_t read_count = count_reads(std::make_index_sequence<arity>{});
    static constexpr std::size_t write_count = count_writes(std::make_index_sequence<arity>{});
    static constexpr bool needs_world_read = any_world_read(std::make_index_sequence<arity>{});
    static constexpr bool needs_world_write = any_world_write(std::make_index_sequence<arity>{});
    static constexpr bool is_exclusive = needs_world_write;
};

} // namespace dod
