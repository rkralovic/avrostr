#ifndef __WINDOW_H_
#define __WINDOW_H_

#include <cairo-pdf.h>
#include <cairo-xcb.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_image.h>

#include <functional>

using DrawFunction = std::function<void(cairo_t *)>;

struct Window {
  int width, height;
  double xmin, xmax, ymin, ymax, brdx, brdy; // size + border
  double ratio; // ratio of sides

  xcb_connection_t *c;
  cairo_surface_t *surface;
  cairo_t *cr;

  Window(double _xmin = 0, double _xmax = 1, double _ymin = 0, double _ymax = 1,
         double _brdx = 0.3, double _brdy = 0.3);
  ~Window();

  DrawFunction draw; // called when drawing

  void redraw();
  void show(bool wait);
  void savePDF(const char *fname, double pts = 500);

  private:
  xcb_visualtype_t *find_visual(xcb_connection_t *c, xcb_visualid_t visual);
};

#endif
