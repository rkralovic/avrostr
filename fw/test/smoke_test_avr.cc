#include "avr_mcu_section.h"

// There are multiple problems with AVR_MCU macro (section is dropped by
// linker, it uses 'string' as designator, which gcc does not like.)
// AVR_MCU(F_CPU, "atmega328");

#define MMCU __attribute__((used,section(".mmcu")))
const struct avr_mmcu_string_t _AVR_MMCU_TAG_NAME MMCU = {
        AVR_MMCU_TAG_NAME,
        sizeof(struct avr_mmcu_string_t) - 2,
        "atmega328",
};

#define USED __attribute__((used))

#include "../driver.h"
#include "../../gendata/image-a4.h"

// Input: To be updated by host
volatile uint32_t cycle_count USED;

// Output: To be filled by this code (simulated AVR)
volatile uint8_t state USED;

// This is used to guarante atomic reads from cycle_count.
volatile bool cycle_count_lock USED = false;

volatile bool servo_on USED = false;
volatile uint16_t servo_state USED;

constexpr int kNumCoils = 4;
volatile bool left_coils[kNumCoils] USED;
volatile int32_t left_steps;
volatile bool right_coils[kNumCoils] USED;
volatile int32_t right_steps;

CalibrationData kCalibrationData EEMEM {
  .angle_offset = 0,
  .left_fraction = 1 << 14,
  .right_fraction = 1 << 14,
  .pen_down = 1400,
  .pen_up = 800,
};

struct Timer {
  static void Init() {}
  static uint16_t GetTime() {
    cycle_count_lock = true;
    uint16_t retval = cycle_count & 0xffff;
    cycle_count_lock = false;
    return retval;
  }
};

struct Servo {
  void Init() {}
  void Off() {
    servo_on = false;
  }
  void Set(uint16_t v) {
    servo_on = true;
    servo_state = v;
  }
};

template <volatile bool* value>
struct Gpio {
  void ConfigureOutput() {}
  void Set(bool v) {
    *value = v;
  }
};

template<typename Stepper>
struct DebugStepper {
  DebugStepper(volatile int32_t* steps, Stepper stepper)
      : steps_(steps), stepper_(std::move(stepper)) {}

  void Init() {
    stepper_.Init();
  }

  void Off() {
    stepper_.Off();
  }

  void Move(int8_t step) {
    stepper_.Move(step);
    *steps_ = *steps_ + step;
  }

  volatile int32_t* steps_;
  Stepper stepper_;
};

Driver driver{
    Timer(),
    DebugStepper(&left_steps,
                 Stepper(List(Gpio<&left_coils[0]>(), Gpio<&left_coils[1]>(),
                              Gpio<&left_coils[2]>(), Gpio<&left_coils[3]>()))),
    DebugStepper(
        &right_steps,
        Stepper(List(Gpio<&right_coils[0]>(), Gpio<&right_coils[1]>(),
                     Gpio<&right_coils[2]>(), Gpio<&right_coils[3]>()))),
    Servo(),
    &kCalibrationData};

int main() {
  state = 1;
  auto intr = []() { return false; };
  // driver.TestDrive(intr);
  // driver.DrawImage(intr, &kJuraj);
  driver.Calibration(intr);
  /*
  driver.Forward(intr, 100000);
  for (uint8_t i = 0; i < 101; ++i) {
    driver.RotateSteps(intr, 31);
  }
  driver.Forward(intr, 100000);
  */
  state = 2;
  return 0;
}
