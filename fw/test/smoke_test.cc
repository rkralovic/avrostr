#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>

#include <string>

#include "window.h"

namespace testing {

class AvrTest : public Test {
 protected:
  static constexpr uint32_t ELF_DATA_OFFSET = 0x800000;

  AvrTest(const std::string& testname)
      : fw_filename_(std::string("build/") + testname + std::string(".elf")) {}

  void SetUp() override {
    elf_firmware_t fw_;
    elf_read_firmware(fw_filename_.c_str(), &fw_);
    for (unsigned int i = 0; i < fw_.symbolcount; ++i) {
      avr_symbol_t* sym = fw_.symbol[i];
      symbols_[sym->symbol] = sym->addr;
      // printf("0x%08x: %s\n", sym->addr, sym->symbol);
    }
    EXPECT_EQ(std::string(fw_.mmcu), "atmega328");
    avr_ = avr_make_mcu_by_name(fw_.mmcu);
    avr_init(avr_);
    avr_load_firmware(avr_, &fw_);
    avr_state_ = GetVar<uint8_t>("state");
    avr_cycle_count_ = GetVar<uint32_t>("cycle_count");
    avr_cycle_count_lock_ = GetVar<bool>("cycle_count_lock");
  }

  template <typename T, int num = 1>
  T* GetVar(const std::string& var) {
    T* ptr = [&]() -> T* {
      auto it = symbols_.find(var);
      if (it == symbols_.end()) return nullptr;
      uint32_t addr = it->second;
      if (addr < ELF_DATA_OFFSET) return nullptr;
      addr -= ELF_DATA_OFFSET;
      if (addr + num * sizeof(T) >= avr_->ramend) return nullptr;
      return reinterpret_cast<T*>(&avr_->data[addr]);
    }();
    EXPECT_TRUE(ptr != nullptr);
    return ptr;
  }

  int Run() {
    int state = cpu_Running;
    *avr_state_ = 0;
    while (state != cpu_Done && state != cpu_Crashed && *avr_state_ == 0) {
      if (!*avr_cycle_count_lock_) {
        // Do not update in the middle of a read.
        *avr_cycle_count_ = avr_->cycle;
      }
      state = avr_run(avr_);
      StepDone();
    }
    return state;
  }

  virtual void StepDone() {}

  const std::string fw_filename_;
  const std::string out_filename_;
  std::map<std::string, uint32_t> symbols_;
  avr_t* avr_;

  uint8_t* avr_state_;
  uint32_t* avr_cycle_count_;
  bool* avr_cycle_count_lock_;
};

class Wheel {
 public:
  // f is maximum force of a single coil
  Wheel(int num_coils, bool* coils, int32_t* requested_steps,
        double l, double r, double u, double d,
        double m, double f, uint32_t t_0, double buffer)
      : NumCoils(num_coils),
        coils_(coils),
        requested_steps_(requested_steps),
        L(l),
        R(r),
        U(u),
        D(d),
        Do(2 * d),
        P(num_coils * d),
        M(m),
        Fr(0.1 * f),
        last_update_(t_0),
        i_(num_coils, 0.0),
        max_buffer_(buffer) {
    double max_i = U / R;

    double max_r;

    /*
    max_r = Do * sqrt(1.5);
    double alpha1 = (f / max_i) * max_r * max_r * max_r / sqrt(max_r * max_r - Do * Do);
    */

    max_r = sqrt(d * d + Do * Do);
    double alpha2 = (f / max_i) * max_r * max_r * max_r / sqrt(max_r * max_r - Do * Do);

    //alpha_ = (alpha1 + alpha2) * 0.5;  // approximate guess to compensate for further positions.
    alpha_ = alpha2;
//    printf("alpha: f = %.6lf, maxi = %.6lf, maxr = %.6lf, Do = %.6lf\n", f, max_i, max_r, Do);
  }

  const bool* Coils() const {
    return coils_;
  }

  int32_t RequestedSteps() const {
    return *requested_steps_;
  }

  double Position() const {
    return s_final_;
  }

  double Velocity() const {
    return v_final_;
  }

  double Current(int c) const {
    return i_[c];
  }

  void Update(uint32_t t) {
    double dt = (t - last_update_) / static_cast<double>(F_CPU);
    last_update_ = t;

    // Update currents through coils.
    for (int c = 0; c < NumCoils; ++c) {
      double u = -i_[c] * R;
      if (coils_[c]) u += U;
      i_[c] += dt * u / L;
      // This could be done more precisely (based on the exponential solution),
      // but we don't need that.
    }

    double f = -v_ * Fr;  // very small friction force dependent on velocity
    if (v_ > 0) {
      f += -Fr;
    } else if (v_ <= 0) {
      f += Fr;
    }

    // Calculate force of coils.
    for (int c = -NumCoils; c <= 2 * NumCoils; ++c) {
      double x = (c * D) - s_;
      if (fabs(x) > P / 2) continue;

      double r = sqrt(x * x + Do * Do);

      int idx = (c + NumCoils) % NumCoils;
      f += i_[idx] * alpha_ * x / (r * r * r);
//      printf("coil %d: x = %.6lf, r = %.6lf, f = %.6lf\n", c, x, r, f);
    }

    double a = f / M;
    double ds = v_ * dt + 0.5 * a * dt * dt;
    s_ += ds;
    buffer_ += ds;
    buffer_ = std::min(buffer_, max_buffer_);
    buffer_ = std::max(buffer_, -max_buffer_);
    double s_prev = s_final_;
    s_final_ = s_ + P * periods_ - buffer_;
    v_final_ = (s_final_ - s_prev) / dt;

    v_ += a * dt;

    if (s_ < 0.0) {
      s_ += P;
      periods_--;
    } else if (s_ > P) {
      s_ -= P;
      periods_++;
    }
  }

 private:

  const int NumCoils;
  bool* coils_;
  int32_t* requested_steps_;

  const double L;  // inductance of the coil
  const double R;  // resistance of the coil
  const double U;  // voltage when the coil is on
  const double D;  // distance between coils.
  const double Do; // offset distance between coil and stator, assume 0.2 * D
  const double P;  // perimeter = NumCoils * D
  const double M;  // mass
  const double Fr;   // Friction force
  double alpha_;  // F = I * alpha / r^2

  uint32_t last_update_;

  double s_ = 0.0;
  int32_t periods_ = 0;
  double v_ = 0.0;
  std::vector<double> i_;

  double v_final_ = 0.0;
  double s_final_ = 0.0;
  double buffer_ = 0;
  double max_buffer_;
};

struct Point {
  double x = 0.0;
  double y = 0.0;
  double alpha = M_PI / 2.0;
  bool pendown = true;
};

class DrawingTest : public AvrTest {
 protected:
  DrawingTest(const std::string& testname)
      : AvrTest(testname),
        out_filename_(std::string("build/") + testname + std::string(".pdf")) {}

  void SetUp() override {
    AvrTest::SetUp();
    servo_on_ = GetVar<bool>("servo_on");
    servo_state_ = GetVar<uint16_t>("servo_state");

    left_ = std::make_unique<Wheel>(kNumCoils, GetVar<bool>("left_coils"),
        GetVar<int32_t>("left_steps"),
        0.022 /*220mH*/, 50.0 /*Ohm*/, 5.0 /*V*/, 0.000077466 /*78um*/,
        0.05 /*50g*/, 1.36 /*N*/, avr_->cycle, 1e-8);
    right_ = std::make_unique<Wheel>(kNumCoils, GetVar<bool>("right_coils"),
        GetVar<int32_t>("right_steps"),
        0.022 /*220mH*/, 50.0 /*Ohm*/, 5.0 /*V*/, 0.000077466 /*78um*/,
        0.05 /*50g*/, 1.36 /*N*/, avr_->cycle, 1e-8);

    last_print_ = last_cycle_ = avr_->cycle;

    // A3 paper, 10mm borders
    window_ =
        std::make_unique<Window>(-0.1485, 0.1485, -0.210, 0.210, 0.010, 0.010);
    window_->draw = [&](cairo_t *cr) {
      cairo_set_line_width(cr, 0.0005);  // 0.5mm tip
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      cairo_set_source_rgb(cr, 1, 0, 0);
      bool pendown = true;
      cairo_move_to(cr, 0, 0);
      for (const Point& p : trace_) {
        if (pendown != p.pendown) {
          cairo_stroke(cr);
          pendown = p.pendown;
          if (pendown) {
            cairo_set_source_rgb(cr, 1, 0, 0);
          } else {
            cairo_set_source_rgb(cr, 0.8, 0.8, 1);
          }
          cairo_move_to(cr, p.x, p.y);
        } else {
          cairo_line_to(cr, p.x, p.y);
        }
      }
      cairo_stroke(cr);

      if (!trace_.empty()) {
        constexpr double kArrowLength = 0.020;   // 20mm
        const Point& last = trace_[trace_.size() - 1];
        cairo_set_source_rgb(cr, 0, 1, 0);
        cairo_move_to(cr, last.x, last.y);
        cairo_line_to(cr, last.x + cos(last.alpha) * kArrowLength,
                      last.y + sin(last.alpha) * kArrowLength);
        cairo_stroke(cr);
      }

    };

    trace_.clear();
    length_since_trace_ = 0.0;
  }

  void TearDown() override {
    window_->show(true);
    window_->savePDF(out_filename_.c_str());
  }

  void StepDone() override {
    left_->Update(avr_->cycle);
    right_->Update(avr_->cycle);

    double v = 0.5 * (right_->Velocity() - left_->Velocity());
    double dt = static_cast<double>(avr_->cycle - last_cycle_) / F_CPU;

    position_.x += v * cos(position_.alpha) * dt;
    position_.y += v * sin(position_.alpha) * dt;

    double omega = (right_->Velocity() + left_->Velocity()) / kWheelDistance;
    position_.alpha += omega * dt;

    if (*servo_on_) {
      position_.pendown = *servo_state_ > 1300;
    }

    length_since_trace_ +=
        (fabs(left_->Velocity()) + fabs(right_->Velocity())) * dt;
    last_cycle_ = avr_->cycle;

    // Update output after each 2 mm
    if (length_since_trace_ > 0.002 ||
        (!trace_.empty() && trace_.back().pendown != position_.pendown)) {
      trace_.push_back(position_);
      window_->redraw();
      length_since_trace_ = 0.0;
    }
    window_->show(false);

    // Debug state each 100ms
    if (last_cycle_ - last_print_ > F_CPU / 10) {
      last_print_ = last_cycle_;
      printf("%9ld: ", avr_->cycle);
      printf(" [%.8lf, %.8lf, %.8lf] ", position_.x, position_.y,
             position_.alpha / 3.14159 * 180.0);
      if (*servo_on_) {
        printf("%5d", *servo_state_);
      } else {
        printf("(off)");
      }
      for (const auto* c : {left_.get(), right_.get()}) {
        printf("    ");
        for (int i = 0; i < 4; ++i) {
          printf(" %s,%.3lf", c->Coils()[i] ? " on": "off", c->Current(i));
        }
        printf("  %.6lf %.3lf", c->Position(), c->Velocity());
        printf("  avg speed: %.3lf", c->Position() / avr_->cycle * F_CPU);
        printf("  position ratio: %.3lf vs. %.2lf", c->Position() / 0.000077466,
               c->RequestedSteps() / 2.0);
      }
      printf("\n");
    }
  }

  static constexpr double kWheelDistance = 0.0772;
// With error:
//  static constexpr double kWheelDistance = 0.08;

  const std::string out_filename_;
  bool* servo_on_;
  uint16_t* servo_state_;

  static constexpr int kNumCoils = 4;
  std::unique_ptr<Wheel> left_;
  std::unique_ptr<Wheel> right_;

  Point position_;
  uint32_t last_cycle_;
  uint32_t last_print_;

  std::unique_ptr<Window> window_;
  std::vector<Point> trace_;
  double length_since_trace_;
};

class SmokeTest : public DrawingTest {
 protected:
  SmokeTest() : DrawingTest("smoke_test") {}
};

TEST_F(SmokeTest, Smoke) {
  EXPECT_EQ(Run(), cpu_Running);
  EXPECT_EQ(*avr_state_, 1);

  EXPECT_EQ(Run(), cpu_Running);
  EXPECT_EQ(*avr_state_, 2);
}

}  // namespace testing
