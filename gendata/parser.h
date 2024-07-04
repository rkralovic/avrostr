#ifndef __PARSER_H__
#define __APRSER_H__

#include <tinyxml2.h>

#include <utility>
#include <vector>

#include "point.h"

using Path = std::vector<Point>;
using Layer = std::vector<Path>;

// bopunding box
struct BBox {
  Point lo, hi;
  BBox(const Path& p = {});
  BBox& operator<<(const Path& p);
};


// parse svg and return sequence of layers
class Parser {
  public:
  double smoothDelta=0.4, elimShortDelta=0.8, joinDelta=0.9;  
  Parser(const char* fname);
  std::vector<Layer> operator()(bool swap_horiz = true);

  private:
  tinyxml2::XMLDocument doc;

  // RamerDouglasPeucker
  void smooth(Path&);

  // remove short segments
  void elimShort(Path&);

  // greedy "TSP": sort paths in layer, start from given point
  // return endpoint
  Point arrange(Layer&, Point);

  // join paths with common endpoints
  void join(Layer&);
};


// bezier curve
struct Bezier3 {
  Point p[4];  // end + control points

  // position and derivative for given t in [0,1]
  double x(double t) const { return val(t, 0); }
  double y(double t) const { return val(t, 1); }
  double dx(double t) const { return diff(t, 0); }
  double dy(double t) const { return diff(t, 1); }

  // internal for position
  double val(double t, int i) const {
    double tt = t * t, ttt = tt * t;
    double s = 1 - t, ss = s * s, sss = ss * s;
    return sss * p[0][i] + 3 * ss * t * p[1][i] + 3 * s * tt * p[2][i] +
           ttt * p[3][i];
  }

  // internal for derivative
  double diff(double t, int i) const {
    double tt = t * t;
    double s = 1 - t, ss = s * s;
    return 3 * ss * (p[1][i] - p[0][i]) + 6 * s * t * (p[2][i] - p[1][i]) +
           3 * tt * (p[3][i] - p[2][i]);
  }
};

#endif
