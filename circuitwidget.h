#ifndef CIRCUITWIDGET__INCLUDED
#define CIRCUITWIDGET__INCLUDED

#include <gtkmm/drawingarea.h>
#include <circuit.h>

class CircuitWidget : public Gtk::DrawingArea {
public:
  CircuitWidget ();
  virtual ~CircuitWidget ();
	void load (string);

  void set_window (Gtk::Window *);
	void set_offset (int);
protected:
  //Override default signal handler:
  virtual bool on_expose_event(GdkEventExpose* event);

private:
  Gtk::Window *win;
	int yoffset;
  Circuit *circuit;
	cairo_rectangle_t ext;
	double wirestart, wireend;
};

#endif
