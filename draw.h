#include <cairo.h>
#include <qc.h>

cairo_rectangle_t get_circuit_size (Circuit *c, double* wirestart, double* wireend, double scale);
cairo_surface_t* make_png_surface (cairo_rectangle_t ext);
cairo_surface_t* draw_circuit (Circuit *c, cairo_t* cr, bool drawArch, bool drawParallel, cairo_rectangle_t ext, double wirestart, double wireend, double scale);
void write_to_png (cairo_surface_t* surf, string filename);
