#include "circuit.h"
#include <cairo.h>
#include <cmath>
#include <iostream>
#include <vector>
using namespace std;

float radius = 15.0;
float dotradius = 10.0;
float thickness = 2.0;
float xoffset = 10.0;
float yoffset = 10.0;
float wireDist = 40.0;
float gatePad = 18.0;

struct gateRect {
  float x0, y0;
	float width, height;
};

class Colour {
  public:
	  Colour () {}
		Colour (float rr, float gg, float bb, float aa) : r(rr), b(bb), g(gg), a(aa) {}
		float r, g, b, a;
};

float wireToY (int x) {
  return yoffset+(x+1)*wireDist;
}

void drawDot (cairo_t *cr, float xc, float yc, float radius, float thickness, bool negative) {
  if (negative) {
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_arc (cr, xc, yc, radius, 0, 2*M_PI);
    cairo_fill (cr);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width(cr, thickness);
    cairo_arc (cr, xc, yc, radius, 0, 2*M_PI);
    cairo_stroke (cr);
  } else {
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_arc (cr, xc, yc, radius, 0, 2*M_PI);
    cairo_fill (cr);
  }
}


void drawBox (cairo_t *cr, string name, float xc, float yc, float height, float thickness, float minpad) {

  // get width of this box
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 35);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, name.c_str(), &extents);

  cairo_rectangle (cr, xc-extents.width/2-minpad, yc-(height+0*extents.height)/2, extents.width+2*minpad, height);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill(cr);
  cairo_rectangle (cr, xc-extents.width/2-minpad, yc-(height+0*extents.height)/2, extents.width+2*minpad, height);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_set_line_width (cr, thickness);
  cairo_stroke(cr);

  float x = xc - (extents.width/2 + extents.x_bearing);
  float y = yc - (extents.height/2 + extents.y_bearing);
  cairo_move_to(cr, x, y);
  cairo_show_text (cr, name.c_str());
}

void drawNOT (cairo_t *cr, float xc, float yc, float radius, float thickness) {
  // Draw white background
  cairo_set_line_width (cr, thickness);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_arc (cr, xc, yc, radius, 0, 2*M_PI);
  cairo_fill (cr);

  // Draw black border
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_arc (cr, xc, yc, radius, 0, 2*M_PI);
  cairo_stroke (cr);

  // Draw cross
  cairo_move_to (cr, xc-radius, yc);
  cairo_line_to (cr, xc+radius, yc);
  cairo_stroke (cr);
  cairo_move_to (cr, xc, yc-radius);
  cairo_line_to (cr, xc, yc+radius);
  cairo_stroke (cr);
}

void drawWire (cairo_t *cr, float x1, float y1, float x2, float y2, float thickness) {
  cairo_set_line_width (cr, thickness);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_stroke (cr);
}

//for parallism wires
void drawPWire (cairo_t *cr, float x, int numLines, float thickness) {
  cairo_set_line_width (cr, thickness);
  cairo_set_source_rgba (cr, 0.4, 0.4, 0.4,0.4);
  cairo_move_to (cr, x, wireToY(0));
  cairo_line_to (cr, x, wireToY(numLines-1));
  cairo_stroke (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
}

gateRect drawCNOT (cairo_t *cr, unsigned int xc, vector<Control> *ctrl, vector<int> *targ) {
	int maxw = (*targ)[0];
	int minw = (*targ)[0];
  for (int i = 0; i < ctrl->size(); i++) {
		minw = min (minw, (*ctrl)[i].wire);
		maxw = max (maxw, (*ctrl)[i].wire);
    drawDot (cr, xc, wireToY((*ctrl)[i].wire), dotradius, thickness, (*ctrl)[i].polarity);
  }
  for (int i = 0; i < targ->size(); i++) {
		minw = min (minw, (*targ)[i]);
		maxw = max (maxw, (*targ)[i]);
    drawNOT (cr, xc, wireToY((*targ)[i]), radius, thickness);
  }
  if (ctrl->size() > 0)drawWire (cr, xc, wireToY (minw), xc, wireToY (maxw), thickness);
	gateRect rect;
	rect.x0 = xc - (radius + thickness);
	rect.y0 = wireToY(minw) - (radius+thickness);
  rect.width = 2.0*(thickness+radius);
  rect.height = wireToY(maxw) - wireToY(minw) + 2.0*(radius+thickness);
	return rect;
}

void drawbase (cairo_surface_t *surface, Circuit *c, float w, float h, float wirestart, float wireend, double scale) {
  cairo_t *cr = cairo_create (surface);
	cairo_scale (cr, scale, scale);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr, 0, 0, w/scale, h/scale);
	cairo_fill (cr);

  for (int i = 0; i < c->numLines(); i++) {
		float y = wireToY (i);
    drawWire (cr, wirestart, y, wireend, y, thickness);
	}
	cairo_destroy (cr);
}

vector<gateRect> draw (cairo_surface_t *surface, Circuit* c, double *wirestart, double *wireend, bool forreal, double scale) {
	vector <gateRect> rects;

	cairo_t *cr = cairo_create (surface);
	cairo_scale (cr, scale, scale);
	cairo_set_source_surface (cr, surface, 0.0, 0.0);

	cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, scale*18);
	cairo_set_source_rgb (cr, 0, 0, 0);

  // input labels
	double xinit = 0.0;
  for (int i = 0; i < c->numLines(); i++) {
		Line *line = c->getLine (i);
    string label = line->getInputLabel ();
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label.c_str(), &extents);

    double x, y;
    if (forreal) {
			x = *wirestart - extents.width;
			y = wireToY(i) - (extents.height/2 + extents.y_bearing);
		}
		cairo_move_to(cr, x, y);
    cairo_show_text (cr, label.c_str());
		xinit = max (xinit, extents.width);
  }

	if (!forreal) *wirestart = xinit;

  // gates
	float xcurr = xinit+2.0*gatePad;
	for (int i = 0; i < c->numGates (); i++) {
		Gate* g = c->getGate (i);
	  gateRect r = drawCNOT (cr, xcurr, &g->controls, &g->targets);
		xcurr += r.width + gatePad;
		rects.push_back(r);
	}
  *wireend = xcurr-gatePad;

  // output labels
	for (int i = 0; i < c->numLines (); i++) {
    Line *line = c->getLine (i);
		string label = line->getOutputLabel();
		cairo_text_extents_t extents;
		cairo_text_extents (cr, label.c_str(), &extents);

		double x, y;
		x = *wireend + xoffset;
		y = wireToY(i) - (extents.height/2+extents.y_bearing);
	  cairo_move_to (cr, x, y);
		cairo_show_text (cr, label.c_str());
	}
  cairo_destroy (cr);

	return rects;
}

void drawRect (cairo_t *cr, gateRect r, Colour outline, Colour fill) {
  cairo_set_source_rgba (cr, fill.r, fill.g, fill.b, fill.a);
	cairo_rectangle (cr, r.x0, r.y0, r.width, r.height);
	cairo_fill (cr);
	cairo_set_source_rgba (cr, outline.r, outline.g, outline.b, outline.a);
	cairo_rectangle (cr, r.x0, r.y0, r.width, r.height);
	cairo_stroke (cr);
}

void drawArchitectureWarnings (cairo_surface_t* surface, vector<gateRect> rects, vector<int> badGates, double scale) {
	cairo_t *cr = cairo_create (surface);
	cairo_scale (cr, scale, scale);
	cairo_set_source_surface (cr, surface, 0.0, 0.0);
  for (int i = 0; i < badGates.size(); i++) {
    drawRect (cr, rects[badGates[i]], Colour(0.8,0.1,0.1,0.7), Colour(0.8,0.4,0.4,0.3));
	}
	cairo_destroy (cr);
}

void drawParallelSectionMarkings (cairo_surface_t* surface, vector<gateRect> rects, int numLines, vector<int> pLines, double scale) {
	cairo_t *cr = cairo_create (surface);
	cairo_scale (cr, scale, scale);
	cairo_set_source_surface (cr, surface, 0.0, 0.0);
	for (int i = 0; i < pLines.size(); i++) {
		int gateNum = pLines[i];
		float x = rects[gateNum].x0 + rects[gateNum].width + gatePad/2;
    drawPWire (cr, x, numLines,thickness);
	}
  cairo_destroy (cr);
}

cairo_surface_t* make_png_surface (cairo_rectangle_t ext) {
  cairo_surface_t *img_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, ext.width+ext.x, thickness+ext.height+ext.y);
  return img_surface;
}

cairo_rectangle_t get_circuit_size (Circuit *c, double* wirestart, double* wireend, double scale) {
	cairo_surface_t *unbounded_rec_surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR, NULL);
	cout << "Drawing fake cairo image... " << flush;
	draw (unbounded_rec_surface, c, wirestart, wireend, false, scale);
	cout << "ding!\nDrawing wires and labels... " << flush;
	cairo_rectangle_t ext;
	cairo_recording_surface_ink_extents (unbounded_rec_surface, &ext.x, &ext.y, &ext.width, &ext.height);
  cairo_surface_destroy (unbounded_rec_surface);
	return ext;
}

void write_to_png (cairo_surface_t* surf, string filename) {
	cout << "Saving to \"" << filename << "\"..." << flush;
	cairo_surface_write_to_png (surf, filename.c_str());
	cout << "done." << endl << flush;
}

cairo_surface_t* makepicture (Circuit *c, bool drawArch, double scale) {
	vector<gateRect> rects;
	double wirestart, wireend;

  cairo_rectangle_t ext = get_circuit_size (c, &wirestart, &wireend, scale);
	cairo_surface_t* surf = make_png_surface (ext);
	drawbase (surf, c, ext.width+ext.x, ext.height+scale*ext.y+thickness, wirestart+xoffset, wireend, scale);
	rects = draw (surf, c, &wirestart, &wireend, true, scale);
  drawParallelSectionMarkings (surf, rects, c->numLines(),c->getParallel(), scale);
  if (drawArch) drawArchitectureWarnings (surf, rects, c->getArchWarnings(), scale);

  write_to_png (surf, "circuit.png");
	cairo_surface_destroy (surf);

	//////////////////(XXX)= c->getParallel();

}
