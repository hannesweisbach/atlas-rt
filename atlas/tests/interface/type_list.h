#pragma once

struct void_t {};

template <typename T, typename... Ts>
struct type_list : public type_list<Ts...> {
  using type = T;
  using tail = type_list<Ts...>;
};

template <typename T> struct type_list<T> {
  using type = T;
  using tail = type_list<void_t>;
};

template <typename LHS, typename RHS> struct list_concat {
  using type = type_list<LHS, RHS>;
};

template <typename LHS, typename... RHS>
struct list_concat<LHS, type_list<RHS...>> {
  using type = type_list<LHS, RHS...>;
};

template <typename RHS, typename... LHS>
struct list_concat<type_list<LHS...>, RHS> {
  using type = type_list<LHS..., RHS>;
};

template <typename... LHS, typename... RHS>
struct list_concat<type_list<LHS...>, type_list<RHS...>> {
  using type = type_list<LHS..., RHS...>;
};

template <typename... Ts>
struct list_concat<type_list<Ts...>, type_list<void_t>> {
  using type = type_list<Ts...>;
};

template <typename... Ts>
struct list_concat<type_list<void_t>, type_list<Ts...>> {
  using type = type_list<Ts...>;
};

namespace _ {
template <typename... T> struct combination { using type = type_list<T...>; };
}

template <typename... TLists> struct combinator;

template <typename... TLists> struct combinator<type_list<void_t>, TLists...> {
  using type = type_list<void_t>;
};

template <typename... Ts> struct combinator<_::combination<Ts...>> {
  using type = type_list<typename _::combination<Ts...>::type>;
};

template <typename... Ts, typename... TLists>
struct combinator<_::combination<Ts...>, type_list<void_t>, TLists...> {
  using type = type_list<void_t>;
};

template <typename... Ts, typename... TLists>
struct combinator<type_list<Ts...>, TLists...> {
  using head =
      typename combinator<_::combination<typename type_list<Ts...>::type>,
                          TLists...>::type;
  using tail =
      typename combinator<typename type_list<Ts...>::tail, TLists...>::type;
  using type = typename list_concat<head, tail>::type;
};

template <typename... Ts, typename... Us, typename... TLists>
struct combinator<_::combination<Ts...>, type_list<Us...>, TLists...> {
  using head = typename combinator<
      _::combination<Ts..., typename type_list<Us...>::type>, TLists...>::type;
  using tail =
      typename combinator<_::combination<Ts...>,
                          typename type_list<Us...>::tail, TLists...>::type;
  using type = typename list_concat<head, tail>::type;
};

template <template <typename...> class Unit, typename TList> struct apply;

template <template <typename...> class Unit, typename... TLists,
          typename... Ts>
struct apply<Unit, type_list<type_list<Ts...>, TLists...>> {
  using type = Unit<Ts...>;

  static void invoke() {
    type::invoke();
    apply<Unit, type_list<TLists...>>::invoke();
  }
};

template <template <typename...> class Unit, typename... Ts>
struct apply<Unit, type_list<type_list<Ts...>>> {
  using type = Unit<Ts...>;

  static void invoke() { type::invoke(); }
};
