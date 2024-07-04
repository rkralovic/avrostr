#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include <stdint.h>

#include "gpio.h"
#include "driver.h"
#include "motors.h"
#include "utils.h"
#include "stl.h"

#if ID == 1
#include "../gendata/image-a3.h"
#elif ID == 2
#include "../gendata/image-a3.h"
#endif

static_assert(F_CPU == 16000000, "Unexpected CPU frequency.");

// Image data
Image const * const kImages[] PROGMEM = {
  &kExample,
};

constexpr int8_t kNumImages = sizeof(kImages) / sizeof(const Image*);

// Calibration data in eeprom. This should be updated for each robot.
#if ID == 1
CalibrationData kCalibrationData EEMEM {
  .angle_offset = 256,
  .left_fraction = 1 << 14,
  .right_fraction = 1 << 14,
  .pen_down = 1400,
  .pen_up = 800,
};
#endif

static uint16_t kServoPeriod EEMEM = 20000;  // servo update period in us

//
// Low-level functionality
//

// MCU utils
inline void UnlockConfig() {
  CCP = CCP_IOREG_gc;
}

// Delay utils
void DelayUs(uint32_t v) {
  v = (v * (F_CPU / 1000000)) / 4;
  if (v & 0xffff) {
    _delay_loop_2(v & 0xffff);
  }
  for (uint16_t i = v >> 16; i > 0; --i) {
    _delay_loop_2(0xffff);
  }
}

void DelayMs(uint32_t v) {
  DelayUs(v * 1000);
}

// Led utils

template<typename Gpio>
void BlinkNum(Gpio& led, uint8_t num) {
  while (num-- > 0) {
    led.Set(true);
    DelayMs(200);
    led.Set(false);
    DelayMs(200);
  }
}

// Timer utils

// Measure time when cpu is running, cpu clocks.
// This class takes ownership of TCB0.
class Timer {
 public:
  static void Init() {
    TCB0.CCMP = 0xffff;  // full 16-bit period
    TCB0.CTRLA = 0x01;  // enable
  }

  static uint16_t GetTime() {
    return TCB0.CNT;
  }
};
static_assert(IsTimer<Timer>);

template <typename Timer, typename Led, typename Button>
uint8_t SelectNumber(Led* led, Button* button, uint8_t min, uint8_t max,
                     uint8_t curr) {
  auto delay = [&](bool break_value, uint32_t us) -> bool {
    uint16_t begin = Timer::GetTime();
    uint32_t ticks = us * (F_CPU / 1000000);
    while(button->Get() != break_value) {
      uint16_t end = Timer::GetTime();
      uint16_t delta = end - begin;
      if (delta > ticks) return false;
      ticks -= delta;
      begin = end;
    }
    return true;
  };

  auto blink = [&](uint8_t num) -> bool {
    for (uint8_t i = 0; i < num; ++i) {
      led->Set(true);
      if (delay(false, 200000)) {
        led->Set(false);
        return true;
      }
      led->Set(false);
      if (delay(false, 200000)) return true;
    }
    if (delay(false, 1000000)) return true;
    return false;
  };

  uint8_t retries = 5;
  while(retries > 0) {
    if (blink(curr)) {
      if (delay(true, 2000000)) {
        curr += 1;
        if (curr > max) curr = min;
        continue;
      } else {
        return curr;
      }
    }
    retries--;
  }
  return max + 1;
}

// High-current voltage regulator
// PG: gpio for power good pin
// PS: gpio for PS/SYNC pin
// EN: gpio for enable pin
template <typename PG, typename PS, typename EN>
class Power {
 public:
  Power(PG pg, PS ps, EN en)
      : pg_(std::move(pg)), ps_(std::move(ps)), en_(std::move(en)) {}

  void Init() {
    en_.ConfigureOutput();
    en_.Set(false);
    ps_.ConfigureOutput();
    ps_.Set(true);
    pg_.ConfigureInput();
    pg_.SetPullup(false);
    pg_.EnableDigitalInput(BothEdges);
    power_good_ = true;
  }

  void On() {
    pg_.SetPullup(true);
    en_.Set(true);
    // Wait until power is on.
    while(!pg_.Get());
    power_good_ = true;
  }

  void Off() {
    en_.Set(false);
    pg_.SetPullup(false);
    power_good_ = true;
  }

  void Irq() {
    power_good_ = pg_.Get();
  }

  bool Ok() const {
    return power_good_;
  }

 private:
  [[no_unique_address]] PG pg_;
  [[no_unique_address]] PS ps_;
  [[no_unique_address]] EN en_;

  volatile bool power_good_;
};

// Servo

// Global state of Servos.
// This class takes ownership of TCA0.
struct ServoState {
  uint8_t running_instances;
  uint16_t period;  // us period

  void Init() {
    running_instances = 0;
    period = eeprom_read_word(&kServoPeriod);

    static_assert(F_CPU == 1000000 || F_CPU == 16000000);
    if constexpr (F_CPU == 16000000) {
      TCA0.SINGLE.CTRLA = 0x08;  // prescaler 16x
    }
    TCA0.SINGLE.PER = period;
    TCA0.SINGLE.CTRLB = 0x03;  // singleslope mode
  }

  void On() {
    if (running_instances == 0) {
      TCA0.SINGLE.CNT = 1;
      TCA0.SINGLE.CTRLA = TCA0.SINGLE.CTRLA | 0x01;
    }
    running_instances++;
  }

  void Off() {
    running_instances--;
    if (running_instances == 0) {
      TCA0.SINGLE.CTRLA = TCA0.SINGLE.CTRLA & ~0x01;
    }
  }
};
ServoState kServoState;

// C: gpio for servo control
// Pin: convertible to integer, TCA0 output pin (CMPxEN), must correspond to C.
// PORTMUX.TCAROUTEA must be set prior to initialization.
template <typename C, typename Pin>
class Servo {
 public:
  Servo(C c, Pin pin) : c_(std::move(c)), pin_(std::move(pin)) {}

  void Init() {
    c_.ConfigureOutput();
    c_.Set(false);
    running_ = false;
  }

  void Off() {
    if (!running_) return;
    running_ = false;
    (&TCA0.SINGLE.CMP0BUF)[pin_] = 0;
    DelayUs(kServoState.period);
    TCA0.SINGLE.CTRLB = TCA0.SINGLE.CTRLB & ~(1 << (4 + pin_));
    kServoState.Off();
  }

  void Set(uint16_t pulse_us) {
    On();
    (&TCA0.SINGLE.CMP0BUF)[pin_] = pulse_us;
  }

 private:
  void On() {
    if (running_) return;
    running_ = true;
    kServoState.On();
    TCA0.SINGLE.CTRLB = TCA0.SINGLE.CTRLB | (1 << (4 + pin_));
  }

  [[no_unique_address]] C c_;
  [[no_unique_address]] Pin pin_;
  bool running_;
};
static_assert(IsServo<Servo<StaticGpio<A, 1>, StaticValue<uint8_t, 1>>>);

// Peripherals:
StaticGpio<F, 5> left_eye;
StaticGpio<F, 2> right_eye;
StaticGpio<D, 7> button;

Power power{StaticGpio<A, 6>(), StaticGpio<A, 7>(), StaticGpio<C, 0>()};

Driver driver{Timer(),
              Stepper(List(StaticGpio<D, 6>(), StaticGpio<D, 5>(),
                           StaticGpio<D, 4>(), StaticGpio<D, 3>())),
              Stepper{List(StaticGpio<D, 2>(), StaticGpio<D, 1>(),
                           StaticGpio<C, 3>(), StaticGpio<C, 2>())},
              Servo(StaticGpio<C, 1>(), StaticValue<uint8_t, 1>()),
              &kCalibrationData};

List unconnected_pins{
    StaticGpio<F, 0>(), StaticGpio<F, 1>(), StaticGpio<F, 3>(),
    StaticGpio<F, 4>(), StaticGpio<F, 0>(), StaticGpio<A, 0>(),
    StaticGpio<A, 1>(), StaticGpio<A, 2>(), StaticGpio<A, 3>(),
    StaticGpio<A, 4>(), StaticGpio<A, 5>()};

struct UnconnectedPinInitFn {
  template<typename C>
  void operator()(C& c, uint8_t) const {
    c.ConfigureInput();
    c.SetPullup(true);
  }
};

// IRQ handlers
ISR(PORTD_PORT_vect) {
  PORTD.INTFLAGS = 0xff;  // Clear interrupt flag
  sei();
}

ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = 0xff;  // Clear interrupt flag
  sei();
  power.Irq();
}

// Board init
void BoardInit() {
  // Clock setup
  UnlockConfig();
  // Run at 16Mhz. Update F_CPU if changed.
  CLKCTRL.OSCHFCTRLA = 0x1c;

  // Brown-out detector is set up in fuses:
  // Run continuous BOD when awake and 32Hz sampled one in sleep.
  // Use 2.85V as threshold.
  // BODCFG = 0x76;

  // Initialize peripherals
  Timer::Init();
  left_eye.ConfigureOutput();
  right_eye.ConfigureOutput();
  button.ConfigureInput();
  button.SetPullup(true);
  button.EnableDigitalInput(BothEdges);
  power.Init();
  kServoState.Init();
  driver.Init();
  // Route WO1 of TCA0 to gpio C1.
  PORTMUX.TCAROUTEA = 0x02;

  // Enable pullup on unconnected pins
  unconnected_pins.ForEach(UnconnectedPinInitFn());

  sei();
}

// Sleep until button press.
void Sleep() {
  while(true) {
    cli();
    if (!button.Get()) {
      sei();
      return;
    }
    // Sleep: power down, enable
    SLPCTRL.CTRLA = 0x05;
    sei();
    sleep_cpu();
    // disable
    SLPCTRL.CTRLA = 0x00;
    DelayMs(10);
  }
}

// Low-level functionality finished.

//
// High-level logic.
//

enum Mode {
  kCalibration,
  kTest,
  kDrawImage
};

int main() {
  BoardInit();

  BlinkNum(left_eye, 2);
  uint8_t img = 1;
  Mode mode;

  while(true) {
    Sleep();

    power.On();
    right_eye.Set(true);
    while(!button.Get());
    right_eye.Set(false);
    // BlinkNum(left_eye, 3);

    uint8_t sel = SelectNumber<Timer>(&left_eye, &button, 1, kNumImages, img);
    if (sel <= kNumImages) {
      img = sel;
      mode = kDrawImage;
    } else {
      driver.Off();
      power.Off();
      continue;
    }
    BlinkNum(right_eye, sel);

    constexpr uint16_t kMaxConsecutivePowerFailures = 2;
    uint16_t power_failures = 0;

    auto interrupted = [&]() {
      if (!button.Get()) return true;
      if (power_failures > kMaxConsecutivePowerFailures) return true;
      if (!power.Ok()) {
        power_failures++;
      } else {
        power_failures = 0;
      }
      if (power_failures > kMaxConsecutivePowerFailures) return true;
      return false;
    };

    switch(mode) {
      case kCalibration:
        driver.Calibration(interrupted);
        break;
      case kTest:
        driver.TestDrive(interrupted);
        break;
      case kDrawImage:
        const Image* ptr;
        memcpy_P(&ptr, &kImages[img - 1], sizeof(const Image*));
        if (img == kNumImages) {
          img = 1;
        } else {
          img++;
        }
        driver.DrawImage(interrupted, ptr);
        break;
    }

    if (power_failures > kMaxConsecutivePowerFailures) {
      BlinkNum(right_eye, 7);
    }

    while(!button.Get());
    BlinkNum(left_eye, 3);

    driver.Off();
    power.Off();
  }

  return 0;
}
