#include "window.h"

#include <cassert>
#include <iostream>
using namespace std;

xcb_visualtype_t *Window::find_visual(xcb_connection_t *c,
                                      xcb_visualid_t visual) {
  xcb_screen_iterator_t screen_iter =
      xcb_setup_roots_iterator(xcb_get_setup(c));

  for (; screen_iter.rem; xcb_screen_next(&screen_iter)) {
    xcb_depth_iterator_t depth_iter =
        xcb_screen_allowed_depths_iterator(screen_iter.data);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
      xcb_visualtype_iterator_t visual_iter =
          xcb_depth_visuals_iterator(depth_iter.data);
      for (; visual_iter.rem; xcb_visualtype_next(&visual_iter))
        if (visual == visual_iter.data->visual_id) return visual_iter.data;
    }
  }

  return NULL;
}

Window::Window(double _xmin, double _xmax, double _ymin, double _ymax,
               double _brdx, double _brdy)
    : xmin{_xmin}, xmax{_xmax}, ymin{_ymin}, ymax{_ymax}, brdx{_brdx}, brdy{_brdy} {
  c = xcb_connect(NULL, NULL);
  assert(!xcb_connection_has_error(c));

  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
  xcb_window_t window = xcb_generate_id(c);

  uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  uint32_t values[2] = {screen->white_pixel,
                        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_RELEASE |
                            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS |
                            XCB_EVENT_MASK_STRUCTURE_NOTIFY};

  //width = (xmax - xmin + 2 * brdx);
  //height = (ymax - ymin + 2 * brdy);

  ratio = (xmax - xmin + 2 * brdx)/(ymax - ymin + 2 * brdy);

  width = 1000;
  height = width/ratio;

  xcb_create_window(c, XCB_COPY_FROM_PARENT, window, screen->root, 20, 20,
                    width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    screen->root_visual, mask, values);
  xcb_map_window(c, window);
  xcb_flush(c);

  xcb_visualtype_t *visual = find_visual(c, screen->root_visual);
  assert(visual);
  surface = cairo_xcb_surface_create(c, window, visual, width, height);
  cr = cairo_create(surface);
  xcb_flush(c);
  draw = [](cairo_t *) {};
}

Window::~Window() {
  cairo_destroy(cr);
  cairo_surface_finish(surface);
  cairo_surface_destroy(surface);
  xcb_disconnect(c);
}

void Window::redraw() {
  cairo_save(cr);
  cairo_set_source_rgb(cr, 255, 255, 255);
  cairo_paint(cr);

  double dx = xmax - xmin + 2 * brdx, dy = ymax - ymin + 2 * brdy;
  double sx = (double)width / dx, sy = (double)height / dy;
  cairo_translate(cr, brdx * sx, brdy * sy);
  cairo_scale(cr, sx, sy);
  cairo_translate(cr, 0, ymax-ymin);
  cairo_scale(cr, 1, -1);
  cairo_translate(cr, -xmin, -ymin);

  draw(cr);
  cairo_restore(cr);
  cairo_surface_flush(surface);
  xcb_flush(c);
}

void Window::show(bool wait) {
  xcb_generic_event_t *event;
  while ((event = wait ? xcb_wait_for_event(c) : xcb_poll_for_event(c))) {
    switch (event->response_type & ~0x80) {
      case XCB_CONFIGURE_NOTIFY: {
        auto ev = (xcb_configure_notify_event_t *)event;
        width = ev->width;
        height = ev->height;
        if (height*ratio>width) 
          height=width/ratio;
        else 
          width=height*ratio;
        
        cairo_xcb_surface_set_size(surface, width, height);
        redraw();
        break;
      }

      case XCB_KEY_RELEASE: {
        auto ev = (xcb_key_release_event_t *)event;
        switch (ev->detail) {
            /* ESC */
          case 9:
            return;
        }
        break;
      }

      case XCB_EXPOSE:
        /* Avoid extra redraws by checking if this is
         * the last expose event in the sequence
         */
        if (((xcb_expose_event_t *)event)->count != 0) break;

        redraw();
        break;
    }
    free(event);
    xcb_flush(c);
  }
}

void Window::savePDF(const char *fname, double pts) {
  double dx = xmax - xmin + 2 * brdx, dy = ymax - ymin + 2 * brdy;
  auto surf = cairo_pdf_surface_create(fname, pts * dx, pts * dy);
  auto ctx = cairo_create(surf);
  cairo_translate(ctx, pts * brdx, pts * brdy);
  cairo_scale(ctx, pts, pts);
  cairo_translate(ctx, 0, ymax-ymin);
  cairo_scale(ctx, 1, -1);
  cairo_translate(ctx, -xmin, -ymin);
  draw(ctx);
  cairo_show_page(ctx);
  cairo_surface_flush(surf);
  cairo_destroy(ctx);
  cairo_surface_finish(surf);
  cairo_surface_destroy(surf);
}
