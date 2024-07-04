#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <tinyxml2.h>

#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "parser.h"
#include "point.h"

#define DEBUG_SHOW 1

#if DEBUG_SHOW
#include "window.h"

struct Color {
  Color(uint32_t c)
      : r{(double)((c & 0xff0000) >> 16) / 255.0},
        g{(double)((c & 0x00ff00) >> 8) / 255.0},
        b{(double)(c & 0x0000ff) / 255.0} {}
  double r, g, b;
};

std::vector<Color> palette{0x1f78b4, 0x33a02c, 0xe31a1c,
                           0xff7f00, 0x6a3d9a, 0xb15928};

// Set color
void cairo_color(cairo_t* cr, const Color& c) {
  cairo_set_source_rgb(cr, c.r, c.g, c.b);
}

// Draw filled picture
void cairo_dot(cairo_t* cr, Point p, double r = 0.01) {
  cairo_save(cr);
  cairo_new_sub_path(cr);
  cairo_arc(cr, p.x, p.y, r, 0, 2 * M_PI);
  cairo_fill(cr);
  cairo_restore(cr);
}
#endif  // DEBUG_SHOW

struct State {
  Point p;
  double alpha = M_PI / 2.0;
  double fwd_distance = 0.0;
  bool fwd = true;
};

void UpdateState(const Point& dst, State* state, int16_t* len, int16_t* angle) {
  constexpr double kWheelDiameter = 50.5;                    // mm
  constexpr double kWheelDistance = 77.2;                    // mm
  constexpr double kStepLen = M_PI * kWheelDiameter / 4096;  // mm / step
  constexpr double kStepAngle =
      2 * M_PI / 4096 * kWheelDiameter / kWheelDistance;  // radians / step

  constexpr double kMinSteps =
      4.0 / 180.0 * M_PI /
      kStepAngle;  // never perform less than this many steps on a motor.

  Point u = dst - state->p;
  *len = static_cast<uint16_t>(u.Len() / kStepLen + 0.5);
  if (fabs(*len) < kMinSteps) {
    *len = 0;
    *angle = 0;
    return;
  }

  double a = -NormalizeAngle(u.Angle() - state->alpha);

  constexpr double kMaxDistance = 100.0;  // switch directions after 100 mm
  if ((state->fwd && state->fwd_distance > kMaxDistance) ||
      (!state->fwd && state->fwd_distance < -kMaxDistance)) {
    state->fwd = !state->fwd;
  } else if (fabs(a) > M_PI / 2) {  // if there is a good time to switch, switch earlier.
    if ((state->fwd && state->fwd_distance > kMaxDistance / 2.0) ||
        (!state->fwd && state->fwd_distance < -kMaxDistance / 2.0)) {
      state->fwd = !state->fwd;
    }
  }

  if (!state->fwd) {
    a = NormalizeAngle(a + M_PI);
    *len = -(*len);
  }

  *angle = static_cast<int16_t>(a / kStepAngle + 0.5);

  if (fabs(*angle) < kMinSteps) *angle = 0;

  state->alpha += -(*angle) * kStepAngle;

  constexpr int16_t k360 = static_cast<int16_t>(2 * M_PI / kStepAngle + 0.5);
  if (state->alpha > 4 * M_PI) {
    *angle += k360;
    state->alpha += -k360 * kStepAngle;
  } else if (state->alpha < -4 * M_PI) {
    *angle -= k360;
    state->alpha -= -k360 * kStepAngle;
  }

  state->p = state->p + Point::FromAngle(state->alpha) * ((*len) * kStepLen);
  state->fwd_distance += (*len) * kStepLen;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "Usage: %s <input.svg> [-elimShort <double>] [-smooth <double>] "
            "[-join <double>] [-landscape] [-a4 | -a2 | -a1]]\n",
            argv[0]);
    return 1;
  }

  const char* nameroot = "Image";

  Parser parser(argv[1]);
  bool landscape = false;
  int paper = 3;

  for (int i = 2; i < argc; i++)
    if (!strcmp(argv[i], "-elimShort")) {
      i++;
      sscanf(argv[i], "%lf", &parser.elimShortDelta);
    } else if (!strcmp(argv[i], "-smooth")) {
      i++;
      sscanf(argv[i], "%lf", &parser.smoothDelta);
    } else if (!strcmp(argv[i], "-join")) {
      i++;
      sscanf(argv[i], "%lf", &parser.joinDelta);
    } else if (!strcmp(argv[i], "-name")) {
      i++;
      nameroot = argv[i];
    } else if (!strcmp(argv[i], "-landscape")) {
      landscape = true;
    } else if (!strcmp(argv[i], "-a4")) {
      paper = 4;
    } else if (!strcmp(argv[i], "-a2")) {
      paper = 2;
    } else if (!strcmp(argv[i], "-a1")) {
      paper = 1;
    }

  fprintf(stderr, "a%d %s\n", paper, (landscape) ? "landcsape" : "portrait");
  auto layers = parser();

  // generate raw_points
  std::vector<std::pair<Point, bool>> raw_points;

  for (auto& l : layers)
    for (auto& path : l)
      for (int i = 0; i < path.size(); i++)
        raw_points.emplace_back(path[i], i > 0);

  if (raw_points.empty()) {
    fprintf(stderr, "No points\n");
    return 1;
  }

  double minx, miny, maxx, maxy;
  maxx = minx = raw_points[0].first.x;
  maxy = miny = raw_points[0].first.y;
  for (int i = 1; i < raw_points.size(); ++i) {
    minx = std::min(minx, raw_points[i].first.x);
    miny = std::min(miny, raw_points[i].first.y);
    maxx = std::max(maxx, raw_points[i].first.x);
    maxy = std::max(maxy, raw_points[i].first.y);
  }

  Point offset{-(minx + maxx) / 2.0, -(miny + maxy) / 2.0};

  // Scale for A3 paper
  double kBorder = 0.80;
  double scale;
  if (paper == 4) {
    // Scale for A4 paper
    if (landscape)
      scale = std::min(kBorder * 297.0 / (maxx - minx),
                       kBorder * 210.0 / (maxy - miny));
    else
      scale = std::min(kBorder * 210.0 / (maxx - minx),
                       kBorder * 297.0 / (maxy - miny));
  } else if (paper == 3) {
    if (landscape)
      scale = std::min(kBorder * 420.0 / (maxx - minx),
                       kBorder * 297.0 / (maxy - miny));
    else
      scale = std::min(kBorder * 297.0 / (maxx - minx),
                       kBorder * 420.0 / (maxy - miny));
  } else if (paper == 2) {
    if (landscape)
      scale = std::min(kBorder * 840.0 / (maxx - minx),
                       kBorder * 420.0 / (maxy - miny));
    else
      scale = std::min(kBorder * 420.0 / (maxx - minx),
                       kBorder * 840.0 / (maxy - miny));
  } else if (paper == 1) {
    if (landscape)
      scale = std::min(kBorder * 1680.0 / (maxx - minx),
                       kBorder * 840.0 / (maxy - miny));
    else
      scale = std::min(kBorder * 840.0 / (maxx - minx),
                       kBorder * 1680.0 / (maxy - miny));
  }

  State s;
  printf(
      "#ifndef IMAGE_%s_H_\n"
      "#define IMAGE_%s_H_\n"
      "#include <avr/pgmspace.h>\n"
      "#include \"../fw/driver.h\"\n"
      "\n"
      "const DataPoint k%sData[] PROGMEM = {\n", nameroot, nameroot, nameroot);
  {
    std::vector<std::pair<Point, bool>> filtered_points;
    for (const auto& raw_p : raw_points) {
      uint8_t pen = raw_p.second ? 1 : 0;
      int16_t len;
      int16_t angle;
      UpdateState((raw_p.first + offset) * scale, &s, &len, &angle);
      if (len != 0 || angle != 0) {
        printf("  { %d, %d, %d },\n", len, angle, pen);
        filtered_points.push_back(raw_p);
      }
    }
    raw_points = std::move(filtered_points);
  }

  printf(
      "};\n\n"
      "const Image k%s PROGMEM = { %d, k%sData };\n\n"
      "#endif  // IMAGE_%s_H_\n",
      nameroot, raw_points.size(), nameroot, nameroot);

#if DEBUG_SHOW
  {
    BBox bbox;
    for (auto& l : layers)
      for (auto& p : l) bbox << p;

    Window win(bbox.lo.x, 2.3 * bbox.hi.x, bbox.lo.y, bbox.hi.y, 0.5, 0.5);
    win.draw = [&](cairo_t* cr) {
      double lw =
          std::max(bbox.hi.x - bbox.lo.x, bbox.hi.y - bbox.lo.y) / 800.0;
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

      cairo_set_line_width(cr, 1.2 * lw);
      cairo_color(cr, 0);
      double dashes[2] = {10 * lw, 10 * lw};
      Color light(0xcccccc);

      cairo_save(cr);
      cairo_translate(cr, 1.3 * bbox.hi.x, 0);
      cairo_move_to(cr, 0, 0);
      cairo_set_dash(cr, dashes, 2, 0);
      cairo_color(cr, light);
      for (int i = 0; i < raw_points.size(); i++) {
        cairo_set_dash(cr, dashes, (raw_points[i].second) ? 0 : 2, 0);
        cairo_color(cr, (raw_points[i].second) ? 0 : light);
        cairo_line_to(cr, raw_points[i].first.x, raw_points[i].first.y);
        cairo_stroke(cr);
        cairo_move_to(cr, raw_points[i].first.x, raw_points[i].first.y);
      }
      cairo_restore(cr);

      cairo_set_line_width(cr, lw);
      Point start{0.5 * (bbox.hi.x + bbox.lo.x), 0.5 * (bbox.hi.y + bbox.lo.y)};
      cairo_color(cr, light);
      cairo_dot(cr, start, 3 * lw);
      cairo_move_to(cr, start.x, start.y);

      cairo_set_dash(cr, dashes, 0, 0);
      for (int i = 0; i < layers.size(); i++)
        for (auto& p : layers[i])
          if (p.size() > 0) {
            cairo_color(cr, light);
            cairo_line_to(cr, p[0].x, p[0].y);
            cairo_stroke(cr);
            cairo_color(cr, palette[i % palette.size()]);
            cairo_dot(cr, p[0], 1.5 * lw);
            cairo_move_to(cr, p[0].x, p[0].y);
            for (int j = 1; j < p.size(); j++)
              cairo_line_to(cr, p[j].x, p[j].y);
            cairo_stroke(cr);
            cairo_move_to(cr, p.back().x, p.back().y);
          }
    };
    win.show(true);
    win.savePDF("image.pdf");
  }
#endif

  return 0;
}
