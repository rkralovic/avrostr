#ifndef DRIVER_H_
#define DRIVER_H_

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#include "motors.h"
#include "utils.h"
#include "stl.h"

struct DataPoint {
  int16_t len;
  int16_t angle : 15;
  uint8_t pen : 1;
};
static_assert(sizeof(DataPoint) == 4);

struct Image {
  uint16_t num_points;
  const DataPoint* points;  // progmem pointer
};

struct CalibrationData {
  int16_t angle_offset;  // in steps, 8.8 fixed point
  int16_t left_fraction;  // .14 fixed point
  int16_t right_fraction;  // .14 fixed point
  uint16_t pen_down;
  uint16_t pen_up;
};

using CalibrationEepromPtr = CalibrationData*;

template <typename Timer, typename LStepper, typename RStepper, typename Servo>
requires IsTimer<Timer> && IsServo<Servo> && IsStepper<LStepper> &&
    IsStepper<RStepper>
class Driver {
 public:
  Driver(Timer, LStepper lstepper, RStepper rstepper, Servo servo,
         CalibrationEepromPtr calibration)
      : left_stepper_(std::move(lstepper)),
        right_stepper_(std::move(rstepper)),
        servo_(std::move(servo)),
        angle_fraction_(0) {
    eeprom_read_block(&calibration_, calibration, sizeof(CalibrationData));
  }

  void Init() {
    left_stepper_.Init();
    right_stepper_.Init();
    servo_.Init();
    Pen(false);
  }

  void Off() {
    left_stepper_.Off();
    right_stepper_.Off();
    servo_.Off();
  }

  void Pen(bool down) {
    servo_.Set(down ? calibration_.pen_down : calibration_.pen_up);
    pen_ = down;
    DelayUs(200000);  // 200ms
  }

  template <typename Interrupted>
  bool ForwardSteps(const Interrupted& interrupted, int16_t steps) {
    int8_t sign = 1;
    if (steps < 0) {
      steps = -steps;
      sign = -1;
    }
    return Move(interrupted, -sign * calibration_.left_fraction,
                sign * calibration_.right_fraction, steps);
  }

  template <typename Interrupted>
  bool RotateSteps(const Interrupted& interrupted, int16_t steps) {
    // Ad-hoc bias
    angle_fraction_ += calibration_.angle_offset;
    steps += angle_fraction_ / 256;
    angle_fraction_ = angle_fraction_ % 256;
    int8_t sign = 1;
    if (steps < 0) {
      steps = -steps;
      sign = -1;
    }
    bool pen = pen_;
    if (kLiftPenWhenRotating) {
      if (pen) {
        Pen(false);
      }
    }
    bool val = Move(interrupted, -sign * calibration_.left_fraction,
                    -sign * calibration_.right_fraction, steps);
    if (kLiftPenWhenRotating) {
      if (pen) {
        Pen(true);
      }
    }
    return val;
  }

  // Distance um <= 500000 (0.5m)
  template <typename Interrupted>
  bool Forward(const Interrupted& interrupted, int32_t um) {
    return ForwardSteps(interrupted, (um * 4096 + 79325) / 158650);
  }

  // TODO: save remainder of rotation.
  template <typename Interrupted>
  bool Rotate(const Interrupted& interrupted, int16_t minutes) {
    // 4096 steps per rotation
    // D = 50.5mm diameter of wheel
    // L = 77.2 distance between wheels (LB: 75.6, UB 77.6)
    // 4096 * L/D steps per rotation = 21600 minutes
    int16_t d = (static_cast<int32_t>(minutes) * 10000) / 34496;
    return RotateSteps(interrupted, d);
  }

  // `image` is a progmem pointer.
  template <typename Interrupted>
  bool DrawImage(const Interrupted& interrupted, const Image* image_ptr) {
    Image image;
    memcpy_P(&image, image_ptr, sizeof(Image));
    uint8_t pen;
    uint16_t i;
    for (i = 0; i < image.num_points; ++i) {
      DataPoint p;
      memcpy_P(&p, &image.points[i], sizeof(DataPoint));
      if (i == 0 || pen != p.pen) {
        Pen(p.pen);
        pen = p.pen;
      }
      if (!RotateSteps(interrupted, p.angle)) break;
      if (!ForwardSteps(interrupted, p.len)) break;
    }
    Pen(false);
    Off();
    return i >= image.num_points;
  }

  template <typename Interrupted>
  bool TestDrive(const Interrupted& interrupted) {
    Pen(false);

    /* Just drive forward
     */
    for (int i = 0; i < 100; ++i) {
      if (!Forward(interrupted, 300000)) return false;
    }

    Off();
    return true;
  }

  template <typename Interrupted>
  bool Calibration(const Interrupted& interrupted) {
    Pen(true);

    bool val = Forward(interrupted, 200000);
    Pen(false);
    val = val &&
        Rotate(interrupted, 180 * 60);
        Rotate(interrupted, 360 * 60) && Rotate(interrupted, 360 * 60) &&
        Rotate(interrupted, 360 * 60);
    Pen(true);
    val = val && Forward(interrupted, 200000);

    Pen(false);
    Off();
    return val;
  }

 private:
  static constexpr bool kLiftPenWhenRotating = false;

  [[no_unique_address]] LStepper left_stepper_;
  [[no_unique_address]] RStepper right_stepper_;
  [[no_unique_address]] Servo servo_;

  static int16_t FractionalMove(uint16_t s, int16_t fraction, int16_t* position,
                                uint16_t* remainder) {
    int32_t p = static_cast<int32_t>(s) * fraction + *remainder;
    *remainder = p & ((1 << 14) - 1);
    p = p >> 14;
    int16_t ret = p - *position;
    *position = p;
    return ret;
  }

  // Fractions are 14-bit fixed point.
  // Returns false if interrupted.
  template <typename Interrupted>
  bool Move(const Interrupted& interrupted, int16_t left_fraction,
            int16_t right_fraction, uint16_t d) {
    Wheel w;
    uint16_t s = 0;
    int16_t l = 0;
    int16_t r = 0;
    uint16_t start = Timer::GetTime();
    while (s < d) {
      uint16_t end = Timer::GetTime();
      bool braking = ((w.v_ / (2 * Wheel::kMaxA)) * w.v_) >> 48 >= d - s;
      s += w.Update(end - start, braking ? -Wheel::kMaxA : Wheel::kMaxA);
      start = end;

      left_stepper_.Move(FractionalMove(s, left_fraction, &l, &left_remainder_));
      right_stepper_.Move(FractionalMove(s, right_fraction, &r, &right_remainder_));

      if (interrupted()) return false;
    }
    return true;
  }

  void DelayUs(uint32_t t) {
    t *= F_CPU / 1000000;
    uint16_t begin = Timer::GetTime();
    while (true) {
      uint16_t end = Timer::GetTime();
      uint16_t delta = end - begin;
      if (delta > t) break;
      t -= delta;
      begin = end;
    }
  }

  struct Wheel {
    int64_t s_ = 0;  // steps, 48-bit fixed point
    int64_t v_ = 0;  // steps / tick, 48-bit fixed point

    static constexpr int64_t kMaxV = (750ll << 48) / F_CPU;  // 750 step/second
    static constexpr int64_t kMaxA =
        kMaxV / (F_CPU / 10);  // 0 to kMaxV in 100ms

    // Update state for `dt_ticks` with acceleration `a`. Acceleration must be
    // between -kMaxA and +kMaxA. Returns number of steps of the motor.
    int8_t Update(uint16_t dt_ticks, int64_t a) {
      int64_t v2 = v_ + a * dt_ticks;
      if (v2 > kMaxV) {
        v2 = kMaxV;
      } else if (v2 < -kMaxV) {
        v2 = -kMaxV;
      }
      s_ += (v_ + v2) * dt_ticks / 2;
      v_ = v2;
      if (s_ > 1ll << 47) {
        s_ -= 1ll << 48;
        return 1;
      } else if (s_ < -(1ll << 47)) {
        s_ += 1ll << 48;
        return -1;
      }
      return 0;
    }
  };

  bool pen_ = false;

  CalibrationData calibration_;
  int16_t angle_fraction_;
  uint16_t left_remainder_ = 0;
  uint16_t right_remainder_ = 0;
};

#endif  // DRIVER_H_
