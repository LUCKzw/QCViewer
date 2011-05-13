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
  cairo_set_source_rgb (cr, 255, 0, 0);
  cairo_move_to (cr, x, wireToY(0));
  cairo_line_to (cr, x, wireToY(numLines-1));
  cairo_stroke (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
}

void drawCNOT (cairo_t *cr, unsigned int xc, vector<Control> *ctrl, vector<int> *targ) {
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

void draw (cairo_surface_t *surface, Circuit* c, double *wirestart, double *wireend, bool forreal, double scale) {
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
	for (int i = 0; i < c->numGates (); i++) {
		Gate* g = c->getGate (i);
    drawCNOT (cr, 50*(i+1)+xinit, &g->controls, &g->targets);
	}
  *wireend = 50*(c->numGates()+1) + xinit;
	//Parallism Lines
	vector<int> pLines = c->getParallel();
	for (int i = 0; i < pLines.size(); i++) {
    drawPWire (cr,50*(pLines.at(i)+1)+xinit+25,c->numLines(),thickness);
	}

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
}

void makepicture (Circuit *c, double scale) {
	double wirestart, wireend;
	// First, find out how big our circuit drawing will be.
	cairo_surface_t *unbounded_rec_surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR, NULL);
	cout << "Drawing fake cairo image... " << flush;
	draw (unbounded_rec_surface, c, &wirestart, &wireend, false, scale);
	cout << "ding!\nDrawing wires and labels... " << flush;

	// Now, draw to png (XXX) with the right dimensions.
	cairo_rectangle_t ext;
	cairo_recording_surface_ink_extents (unbounded_rec_surface, &ext.x, &ext.y, &ext.width, &ext.height);
  cairo_surface_t *img_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, ext.width+ext.x, ext.height+ext.y);
	drawbase (img_surface, c, ext.width+ext.x, ext.height+scale*ext.y, wirestart+xoffset, wireend, scale);
	cout << "ding!\nDrawing... " << flush;
	draw (img_surface, c, &wirestart, &wireend, true, scale);
	cout << "ding!\nsaving... " << flush;
  cairo_surface_write_to_png(img_surface, "circuit.png");
  cout << "ding!\n" << flush;
  cairo_surface_destroy (unbounded_rec_surface);
	cairo_surface_destroy (img_surface);
}
