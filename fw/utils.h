#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include "stl.h"

//
// General utilities
//

// Static value:
template<typename T, T v>
struct StaticValue {
  operator T() const { return v; }
};

// Heterogenous list utilities
template <typename T, typename... Next>
class List {
 public:
  List(T head, Next... tail)
      : head_(std::move(head)), tail_(std::move(tail)...) {}

  // F is a functor with templated operator ()(uint8_t).
  template <typename F>
  void ForEach(const F& f, uint8_t offset = 0) {
    f(head_, offset);
    tail_.ForEach(f, offset + 1);
  }

  // F is a functor with templated operator ().
  template <typename F>
  void ForItem(int i, const F& f) {
    if (i == 0) f(head_);
    else tail_.ForItem(i - 1);
  }

  constexpr static int Len() {
    return 1 + decltype(tail_)::Len();
  }

 private:
  T head_;
  List<Next...> tail_;
};

template <typename T>
class List<T> {
 public:
  List(T head)
      : head_(std::move(head)) {}

  // F is a functor with templated operator ()(uint8_t).
  template <typename F>
  void ForEach(const F& f, uint8_t offset) {
    f(head_, offset);
  }

  // F is a functor with templated operator ().
  template <typename F>
  void ForItem(uint8_t i, const F& f) {
    if (i == 0) f(head_);
    // else means index is out of range, ignore
  }

  constexpr static int Len() {
    return 1;
  }

 private:
  T head_;
};

// Timer interface

template <typename T>
concept IsTimer = requires {
  { T::Init() } -> std::same_as<void>;
  { T::GetTime() } -> std::same_as<uint16_t>;
};

#endif  // UTILS_H_
