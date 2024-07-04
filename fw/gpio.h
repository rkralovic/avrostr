#ifndef AVR_GPIO_H_
#define AVR_GPIO_H_

#include <avr/io.h>
#include <avr/cpufunc.h>

#include "stl.h"

enum GpioPortId {
  A, C, D, F
};

enum GpioInterruptSense {
  Disable = 0x0,
  BothEdges = 0x1,
  Rising = 0x2,
  Falling = 0x3,
};

template<typename T>
concept IsGpio = requires(T a, bool b, GpioInterruptSense is) {
  { a.ConfigureOutput() } -> std::same_as<void>;
  { a.ConfigureInput() } -> std::same_as<void>;
  { a.Set(b) } -> std::same_as<void>;
  { a.Get() } -> std::same_as<bool>;
  { a.EnableDigitalInput(is) } -> std::same_as<void>;
  { a.DisableDigitalInput() } -> std::same_as<void>;

  // Assumes pin configured as input. May have side effects for output mode.
  { a.SetPullup(b) } -> std::same_as<void>;
};

template<GpioPortId port, uint8_t pin>
class StaticGpio;

class DynamicGpio {
 public:
  DynamicGpio(GpioPortId port, uint8_t pin)
      : port_(port), pin_(pin), mask_(1 << pin) {}
 
  void ConfigureOutput();
  void ConfigureInput();
  void Set(bool value);
  bool Get();
  void EnableDigitalInput(GpioInterruptSense is);
  void DisableDigitalInput();
  void SetPullup(bool value);

 private:
  GpioPortId port_;
  uint8_t pin_;
  uint8_t mask_;
};

// Implementation follows.

namespace internal {
#if defined(__AVR_ATmega328PB__)  // used in simavr
// TODO: implement if needed
struct GpioImpl {
  static inline void ConfigureOutput(GpioPortId /*port*/, uint8_t /*mask*/) {
  }

  static inline void ConfigureInput(GpioPortId /*port*/, uint8_t /*mask*/) {
  }

  static inline void Set(GpioPortId /*port*/, uint8_t /*mask*/, bool /*value*/) {
  }

  static inline bool Get(GpioPortId /*port*/, uint8_t /*mask*/) {
    return false;
  }

  static inline void EnableDigitalInput(GpioPortId /*port*/, uint8_t /*pin*/,
                                        GpioInterruptSense /*is*/) {
  }

  static inline void DisableDigitalInput(GpioPortId /*port*/, uint8_t /*pin*/) {
  }

  static inline void SetPullup(GpioPortId /*port*/, uint8_t /*pin*/, bool /*value*/) {
  }
};
#else
struct GpioImpl {
  static constexpr PORT_t* GetPort(GpioPortId port) {
    switch(port) {
      case A: return &PORTA;
      case C: return &PORTC;
      case D: return &PORTD;
      case F: return &PORTF;
    }
    __builtin_unreachable();
  }

  static constexpr register8_t* GetPinctrl(PORT_t* port, uint8_t pin) {
    return (&port->PIN0CTRL) + pin;
  }

  static inline void ConfigureOutput(GpioPortId port, uint8_t mask) {
    GetPort(port)->DIRSET = mask;
  }

  static inline void ConfigureInput(GpioPortId port, uint8_t mask) {
    GetPort(port)->DIRCLR = mask;
  }

  static inline void Set(GpioPortId port, uint8_t mask, bool value) {
    auto* p = GetPort(port);
    if (value) {
      p->OUTSET = mask;
    } else {
      p->OUTCLR = mask;
    }
  }

  static inline bool Get(GpioPortId port, uint8_t mask) {
    return !!(GetPort(port)->IN & mask);
  }

  static inline void EnableDigitalInput(GpioPortId port, uint8_t pin,
                                        GpioInterruptSense is) {
    auto* pinctrl = GetPinctrl(GetPort(port), pin);
    *pinctrl = ((*pinctrl) & 0xf8) | is;
  }

  static inline void DisableDigitalInput(GpioPortId port, uint8_t pin) {
    auto* pinctrl = GetPinctrl(GetPort(port), pin);
    *pinctrl = ((*pinctrl) & 0xf8) | 0x04;
  }

  static inline void SetPullup(GpioPortId port, uint8_t pin, bool value) {
    auto* pinctrl = GetPinctrl(GetPort(port), pin);
    *pinctrl = ((*pinctrl) & 0xf7) | (value ? 0x08 : 0x00);
  }
};
#endif
}  // namespace internal

template<GpioPortId port, uint8_t pin>
class StaticGpio {
 public:
  void ConfigureOutput() {
    internal::GpioImpl::ConfigureOutput(port, 1 << pin);
  }

  void ConfigureInput() {
    internal::GpioImpl::ConfigureInput(port, 1 << pin);
  }

  void Set(bool value) {
    internal::GpioImpl::Set(port, 1 << pin, value);
  }

  bool Get() {
    return internal::GpioImpl::Get(port, 1 << pin);
  }

  void EnableDigitalInput(GpioInterruptSense is) {
    return internal::GpioImpl::EnableDigitalInput(port, pin, is);
  }

  void DisableDigitalInput() {
    return internal::GpioImpl::DisableDigitalInput(port, pin);
  }

  void SetPullup(bool value) {
    return internal::GpioImpl::SetPullup(port, pin, value);
  }
};

void DynamicGpio::ConfigureOutput() {
  internal::GpioImpl::ConfigureOutput(port_, mask_);
}

void DynamicGpio::ConfigureInput() {
  internal::GpioImpl::ConfigureInput(port_, mask_);
}

void DynamicGpio::Set(bool value) {
  internal::GpioImpl::Set(port_, mask_, value);
}

bool DynamicGpio::Get() {
  return internal::GpioImpl::Get(port_, mask_);
}

void DynamicGpio::EnableDigitalInput(GpioInterruptSense is) {
  return internal::GpioImpl::EnableDigitalInput(port_, pin_, is);
}

void DynamicGpio::DisableDigitalInput() {
  return internal::GpioImpl::DisableDigitalInput(port_, pin_);
}

void DynamicGpio::SetPullup(bool value) {
  return internal::GpioImpl::SetPullup(port_, pin_, value);
}

namespace testing {
static_assert(IsGpio<StaticGpio<A, 1>>);
static_assert(IsGpio<DynamicGpio>);
}  // namespace

#endif  // AVR_GPIO_H_
