#ifndef MOTORS_H_
#define MOTORS_H_

#include <stdint.h>

#include "gpio.h"
#include "stl.h"
#include "utils.h"

// Servo interface

template <typename T>
concept IsServo = requires(T t, uint16_t u16) {
  { t.Init() } -> std::same_as<void>;
  { t.Off() } -> std::same_as<void>;
  { t.Set(u16) } -> std::same_as<void>;
};

// Implementation is board-specific and thus not here.

// Stepper interface

template <typename T>
concept IsStepper = requires(T t, int8_t i8) {
  { t.Init() } -> std::same_as<void>;
  { t.Off() } -> std::same_as<void>;
  { t.Move(i8) } -> std::same_as<void>;
};

// Stepper, working in half-step mode.
// C: List<> of gpio pins driving the coils of the stepper motor.
template <typename C>
class Stepper {
 public:
  Stepper(C c) : c_(std::move(c)) {}

  void Init() {
    c_.ForEach(InitFn());
    c_.ForEach(OffFn());
  }

  void Off() {
    c_.ForEach(OffFn());
  }

  void Move(int8_t delta) {
    pos_ += delta;
    pos_ = (pos_ + kPeriod) % kPeriod;

    c_.ForEach(UpdateFn(kPeriod, pos_));
  }

 private:
  struct InitFn {
    template <typename T>
    void operator()(T& c, uint8_t) const {
      c.ConfigureOutput();
    }
  };

  struct OffFn {
    template <typename T>
    void operator()(T& c, uint8_t) const {
      c.Set(false);
    }
  };

  struct UpdateFn {
    UpdateFn(uint8_t period_, uint8_t pos_) : period(period_), pos(pos_) {}

    template <typename T>
    void operator()(T& c, uint8_t i) const {
      c.Set((pos - 2*i + 1 + period) % period < 3);
    }

    uint8_t period;
    uint8_t pos;
  };

  [[no_unique_address]] C c_;
  constexpr static int kPeriod = 2 * C::Len();
  uint8_t pos_ = 0;
};
static_assert(
    IsStepper<
        Stepper<List<DynamicGpio, DynamicGpio, DynamicGpio, DynamicGpio>>>);

#endif  // MOTORS_H_
