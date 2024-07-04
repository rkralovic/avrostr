#ifndef __POINT_H__
#define __POINT_H__

#include <math.h>

#include <cassert>
#include <ostream>

constexpr double eps = 1e-12;

struct Point {
  double x = 0.0, y = 0.0;
  static Point FromAngle(double a) { return Point{cos(a), sin(a)}; }

  double &operator[](int i) {
    assert(i == 0 || i == 1);
    if (i == 0) return x;
    return y;
  }
  double operator[](int i) const {
    assert(i == 0 || i == 1);
    if (i == 0) return x;
    return y;
  }

  Point operator+(const Point &p) const { return Point{x + p.x, y + p.y}; }
  Point operator-(const Point &p) const { return Point{x - p.x, y - p.y}; }

  Point &operator+=(const Point &p) {
    x += p.x;
    y += p.y;
    return *this;
  }

  Point &operator-=(const Point &p) {
    x += p.x;
    y += p.y;
    return *this;
  }

  Point &operator*=(double c) {
    x *= c;
    y *= c;
    return *this;
  }

  bool operator==(const Point &p) const {
    return fabs(x - p.x) < eps && fabs(y - p.y) < eps;
  }

  double len2() const { return x * x + y * y; }
  double len() const { return sqrt(len2()); }
  double Len() const { return sqrt(x * x + y * y); };

  // angle from x axis
  double Angle() const { return atan2(y, x); }  // (-PI..PI)
  double angle() const {                        //  (0..2*PI)
    if (fabs(x) < eps) return (y > 0) ? M_PI / 2 : 3 * M_PI / 2;
    double res = atan2(y, x);
    if (res < 0) res += 2 * M_PI;
    return res;
  }

  Point &norm() {  // normalize vector
    double l = 1.0 / len();
    if (fabs(l) > eps) {
      x = l * x;
      y = l * y;
    }
    return (*this);
  }

  Point &orth() {  // make orthogonal vector
    y *= -1;
    std::swap(x, y);
    return *(this);
  }

  // rotate a vector by a given angle
  Point &rot(double u) {
    double s = sin(u), c = cos(u), xx = x * c - y * s, yy = x * s + y * c;
    x = xx;
    y = yy;
    return *this;
  }
};

inline Point operator*(const Point &p, double r) {
  return Point{r * p.x, r * p.y};
}
inline Point operator*(double r, const Point &p) {
  return Point{r * p.x, r * p.y};
}

inline std::ostream &operator<<(std::ostream &str, const Point &p) {
  str << "[ " << p.x << ", " << p.y << " ]";
  return str;
}

inline double Mod(double a, double b) {
  double d = floor(a / b);
  a = a - d * b;
  if (a < 0) a += b;
  return a;
}

// dot product
inline double dot(const Point &A, const Point &B) {
  return A.x * B.x + A.y * B.y;
}

// ccw==1 <=>  vector B is ccv from A, -1 cw, 0 colinear
inline int ccw(const Point &A, const Point &B) {
  double c = A.x * B.y - A.y * B.x;
  if (c > eps) return 1;
  if (c < -eps) return -1;
  return 0;
}

inline double deg(double rad) { return rad * 180 / M_PI; }

// return angle from range 0 .. 2*M_PI
inline double wrapAngle(double angle) {
  return angle - 2 * M_PI * floor(angle / (2 * M_PI));
}

// Normalize angle to -M_PI .. M_PI
inline double NormalizeAngle(double a) {
  a = Mod(a, 2 * M_PI);
  if (a > M_PI) a -= 2 * M_PI;
  return a;
}

// Return minimum distance between line segment vw and point p
inline double segDist(Point v, Point w, Point p) {
  const double l2 = (v - w).len2();
  if (l2 == 0.0) return sqrt((p - v).len2());
  // Consider the line extending the segment, parameterized as v + t (w - v).
  // We find projection of point p onto the line.
  // It falls where t = [(p-v) . (w-v)] / |w-v|^2
  // We clamp t from [0,1] to handle points outside the segment vw.
  const double t = std::max(0.0, std::min(1.0, dot(p - v, w - v) / l2));
  const Point projection = v + t * (w - v);
  return sqrt((p - projection).len2());
}


#endif
