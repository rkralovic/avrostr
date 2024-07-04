#include "parser.h"

#include <absl/strings/str_split.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include <algorithm>
#include <cassert>
#include <numeric>
#include <sstream>
#include <string>

#include<iostream>

BBox::BBox(const Path& p) : lo{1e30, 1e30}, hi{-1e30, -1e30} { (*this) << p; }

BBox& BBox::operator<<(const Path& p) {
  for (auto& a : p)
    for (int i : {0, 1}) {
      lo[i] = std::min(lo[i], a[i]);
      hi[i] = std::max(hi[i], a[i]);
    }
  return *this;
}

Parser::Parser(const char* fname) {
  assert(doc.LoadFile(fname) == tinyxml2::XML_SUCCESS);
}

std::vector<Layer> Parser::operator()(bool swap_horiz) {
  std::vector<tinyxml2::XMLNode*> xmlStack;
  assert(doc.RootElement());
  xmlStack.push_back(doc.RootElement()->FirstChild());

  std::vector<Layer> layers(1);
  std::vector<int> curLayer{0};

  while (xmlStack.size() > 0) {
    tinyxml2::XMLNode* node = xmlStack.back();
    xmlStack.pop_back();
    if (!node) {
      curLayer.pop_back();
      continue;
    }
    tinyxml2::XMLElement* e = node->ToElement();
    node = node->NextSibling();
    if (node)
      xmlStack.push_back(node);
    else
      xmlStack.push_back(nullptr);

    if (!strcmp(e->Name(), "polyline")) {
      // polyline
      const char* points_str = e->Attribute("points");
      if (!points_str) {
        fprintf(stderr, "Polyline has no points attribute, ignoring\n");
        continue;
      }

      std::vector<std::string> points_vect = absl::StrSplit(points_str, ",");
      if (points_vect.empty() || points_vect.size() % 2 != 0) {
        fprintf(stderr, "Odd or empty points\n");
        continue;
      }

      auto& paths = layers[curLayer.back()];
      paths.emplace_back(Path{});
      for (int i = 0; i < points_vect.size(); i += 2) {
        double x, y;
        sscanf(points_vect[i].c_str(), "%lf", &x);
        sscanf(points_vect[i + 1].c_str(), "%lf", &y);
        paths.back().emplace_back(Point{x, y});
      }

    } else if (!strcmp(e->Name(), "path")) {
      // parse path
      std::string path_str(e->Attribute("d"));
      std::replace(path_str.begin(), path_str.end(), ',', ' ');
      std::stringstream ss(path_str);
      char op = 'M';
      bool repeat = false;
      Point curr{0, 0}, init{0, 0};
      auto& paths = layers[curLayer.back()];
      while (ss) {
        char c = ss.peek();
        if (isalpha(c)) {
          op = ss.get();
          repeat = false;
          continue;
        }
        switch (op) {
          case 'm': {
            // moveto relative
            double x, y;
            ss >> x >> y;
            curr.x += x;
            curr.y += y;
            if (!repeat) paths.emplace_back(Path{});
            paths.back().emplace_back(curr);
            init = curr;
            break;
          }
          case 'M': {
            // moveto absolute
            ss >> curr.x >> curr.y;
            if (!repeat) paths.emplace_back(Path{});
            paths.back().emplace_back(curr);
            init = curr;
            break;
          }
          case 'c': {
            // cubic_bezier_to relative
            Point c1, c2, dst;
            ss >> c1.x >> c1.y >> c2.x >> c2.y >> dst.x >> dst.y;
            Bezier3 b{{curr, curr + c1, curr + c2, curr + dst}};
            double d = 0.05;
            for (double t = d; t < 1 + 1e-6; t += d)
              paths.back().emplace_back(Point{b.x(t), b.y(t)});
            curr = curr + dst;
            break;
          }
          case 'C': {
            // cubic_bezier_to absolute
            Point c1, c2, dst;
            ss >> c1.x >> c1.y >> c2.x >> c2.y >> dst.x >> dst.y;
            Bezier3 b{{curr, c1, c2, dst}};
            double d = 0.05;
            for (double t = d; t < 1 + 1e-6; t += d)
              paths.back().emplace_back(Point{b.x(t), b.y(t)});
            curr = dst;
            break;
          }
          case 'v': {
            // vertical relative
            double t;
            ss >> t;
            curr.y += t;
            paths.back().emplace_back(curr);
            break;
          }
          case 'V': {
            // vertical absolute
            double t;
            ss >> t;
            curr.y = t;
            paths.back().emplace_back(curr);
            break;
          }
          case 'h': {
            // horizontal relative
            double t;
            ss >> t;
            curr.x += t;
            paths.back().emplace_back(curr);
            break;
          }
          case 'H': {
            // horizontal absolute
            double t;
            ss >> t;
            curr.x = t;
            paths.back().emplace_back(curr);
            break;
          }
          case 'l': {
            // lineto relative
            Point dst;
            ss >> dst.x >> dst.y;
            curr = curr + dst;
            paths.back().emplace_back(curr);
            break;
          }
          case 'z':
          case 'Z': {
            curr = init;
            paths.back().emplace_back(curr);
            break;
          }
          default:
            fprintf(stderr, "%c ", op);
        }
        ss >> std::ws;
        repeat = true;
      }
    } else if (!strcmp(e->Name(), "g")) {
      // group: FIXME ignoring group transformations
      const char* gmode = e->Attribute("inkscape:groupmode");
      if (gmode && !strcmp(gmode, "layer")) {
        // this is a layer
        const char* glabel = e->Attribute("inkscape:label");
        if (glabel) fprintf(stderr, "layer %s  ", glabel);
        layers.push_back(Layer{});
        curLayer.push_back(layers.size() - 1);
      } else {
        fprintf(stderr, "non-layer group  ");
        curLayer.push_back(curLayer.back());
      }
      fprintf(stderr, "(%d)\n", curLayer.back());
      xmlStack.push_back(e->FirstChild());
    } else
      fprintf(stderr, "ignoring unknown element: %s\n", e->Name());
  }

  std::reverse(layers.begin(),layers.end());

  if (swap_horiz)
    for (auto& l : layers)
      for (auto& p : l)
        for (auto& a : p) a.y *= -1;


  BBox bbox;
  for (auto& l : layers)
    for (auto& p : l) bbox << p;
  Point curr = 0.5 * Point{bbox.hi.x + bbox.lo.x, bbox.hi.y + bbox.lo.y};

  for (auto& l : layers) {
    curr = arrange(l, curr);
    join(l);
    for (auto& p : l) {
      smooth(p);
      elimShort(p);
    }
  }

#if 0
  for(auto &l:layers) {
    std::cerr<<"----------------\n";
    for(auto&p:l)if (p.size())
      std::cerr<<p[0]<<" "<<p.back()<<std::endl;
  }
#endif

  return layers;
}

void Parser::smooth(Path& a) {
  double delta=smoothDelta;
  if (a.size() < 3) return;
  Point p = a[0], q = a.back();
  auto dmax = std::max_element(a.begin() + 1, a.end() - 1,
                               [&p, &q](const Point& a, const Point& b) {
                                 return segDist(p, q, a) < segDist(p, q, b);
                               });
  if (segDist(p, q, *dmax) < delta) {
    a.resize(2);
    a[0] = p;
    a[1] = q;
    return;
  }
  int m = std::distance(a.begin(), dmax);
  Path left, right;
  std::copy(a.begin(), a.begin() + m, std::back_inserter(left));
  std::copy(a.begin() + m, a.end(), std::back_inserter(right));
  smooth(left);
  smooth(right);
  a.clear();
  std::copy(left.begin(), left.end() - 1, std::back_inserter(a));
  std::copy(right.begin(), right.end(), std::back_inserter(a));
}

void Parser::elimShort(Path& a) {
  double delta=elimShortDelta;
  if (a.size() < 3) return;
  Path res;
  res.emplace_back(a[0]);
  int last = 0;
  for (int i = 1; i < a.size() - 1; i++) {
    double d = (a[last] - a[i]).len2();
    if (d > delta) {
      last = i;
      res.emplace_back(a[i]);
    }
  }
  res.emplace_back(a.back());
  a = res;
}

Point Parser::arrange(Layer& paths, Point cur) {
  int n = paths.size();
  if (n < 2) return cur;
  std::vector<int> ord(n), rem(n);
  std::iota(rem.begin(), rem.end(), 0);

  for (int i = 0; rem.size() > 0; i++) {
    auto dmin = std::min_element(rem.begin(), rem.end(), [&](int i, int j) {
      Path&a = paths[i], &b = paths[j];
      return std::min((cur - a[0]).len2(), (cur - a.back()).len2()) <
             std::min((cur - b[0]).len2(), (cur - b.back()).len2());
    });

    ord[*dmin] = i;
    if ((cur - paths[*dmin][0]).len2() > (cur - paths[*dmin].back()).len2())
      std::reverse(paths[*dmin].begin(), paths[*dmin].end());
    cur = paths[*dmin].back();

    int j = dmin - rem.begin();
    rem[j] = rem.back();
    rem.pop_back();
  }

  Layer res(n);
  for (int i = 0; i < n; i++) res[ord[i]] = std::move(paths[i]);
  paths = std::move(res);
  return cur;
}

void Parser::join(Layer& paths) {
  double delta=joinDelta;
  if (paths.size() < 2) return;
  Layer res(1);
  res[0] = std::move(paths[0]);
  for (int i = 1; i < paths.size(); i++)
    if ((res.back().back() - paths[i][0]).len() < delta)
      std::copy(paths[i].begin(), paths[i].end(),
                std::back_inserter(res.back()));
    else
      res.push_back(std::move(paths[i]));
  paths = std::move(res);
}

