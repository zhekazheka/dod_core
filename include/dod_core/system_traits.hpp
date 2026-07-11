#pragma once

#include <cstddef>
#include <dod_core/query.hpp>
#include <tuple>
#include <type_traits>

namespace dod
{

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

template <typename T> struct query_kind
{
    static_assert(sizeof(T) == 0,
                  "System parameter must be Read<T>, Write<T>, WorldRead, or WorldWrite");
};

template <typename T> struct query_kind<Read<T>>
{
    using component = T;
    static constexpr bool is_read = true;
    static constexpr bool is_write = false;
    static constexpr bool is_world_read = false;
    static constexpr bool is_world_write = false;

    static Read<T> construct(World& world) { return Read<T>{world}; }
    static void prepare(World& world) { world.registry().storage<T>(); }
};

template <typename T> struct query_kind<Write<T>>
{
    using component = T;
    static constexpr bool is_read = false;
    static constexpr bool is_write = true;
    static constexpr bool is_world_read = false;
    static constexpr bool is_world_write = false;

    static Write<T> construct(World& world) { return Write<T>{world}; }
    static void prepare(World& world) { world.registry().storage<T>(); }
};

template <> struct query_kind<WorldRead>
{
    static constexpr bool is_read = false;
    static constexpr bool is_write = false;
    static constexpr bool is_world_read = true;
    static constexpr bool is_world_write = false;

    static WorldRead construct(World& world) { return WorldRead{world}; }
    static void prepare(World&) {}
};

template <> struct query_kind<WorldWrite>
{
    static constexpr bool is_read = false;
    static constexpr bool is_write = false;
    static constexpr bool is_world_read = false;
    static constexpr bool is_world_write = true;

    static WorldWrite construct(World& world) { return WorldWrite{world}; }
    static void prepare(World&) {}
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
        return ((detail::query_kind<std::tuple_element_t<Is, args_tuple>>::is_read ? 1 : 0) + ... +
                0);
    }

    template <std::size_t... Is>
    static constexpr std::size_t count_writes(std::index_sequence<Is...>)
    {
        return ((detail::query_kind<std::tuple_element_t<Is, args_tuple>>::is_write ? 1 : 0) + ... +
                0);
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
