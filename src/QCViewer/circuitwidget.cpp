/*--------------------------------------------------------------------
QCViewer
Copyright (C) 2011  Alex Parent and The University of Waterloo,
Institute for Quantum Computing, Quantum Circuits Group

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

See also 'ADDITIONAL TERMS' at the end of the included LICENSE file.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

QCViewer is a trademark of the of the The University of Waterloo,
Institute for Quantum Computing, Quantum Circuits Group

Authors: Alex Parent, Jacob Parker
---------------------------------------------------------------------*/

#include "circuitwidget.h"
#include <assert.h>
#include "GateIcon.h"
#include <cairomm/context.h>
#include "QCLib/circuit.h"
#include "QCLib/subcircuit.h"
#include "QCLib/circuitParser.h"
#include "QCLib/simulate.h"
#include <iostream>
#include "draw.h"
#include <gtkmm.h>
#include "QCLib/gate.h"
#include "window.h" // slows down compiles, would be nice to not need this (see: clicking, effects toolpalette)

using namespace std;

CircuitWidget::CircuitWidget()
{
    state = NULL;
    circuit = NULL;
    win = NULL;
    cx = cy = 0;
    panning = drawarch = drawparallel = false;
    mode = NORMAL;
    ext.width=0;
    ext.height=0;
    NextGateToSimulate = 0;
    scale = 1.0;
    add_events (Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |Gdk::SCROLL_MASK);
    signal_button_press_event().connect(sigc::mem_fun(*this, &CircuitWidget::on_button_press_event));
    signal_button_release_event().connect(sigc::mem_fun(*this, &CircuitWidget::on_button_release_event) );
    signal_scroll_event().connect( sigc::mem_fun( *this, &CircuitWidget::onScrollEvent ) );
    signal_motion_notify_event().connect (sigc::mem_fun(*this, &CircuitWidget::onMotionEvent));
    wirestart = wireend = 0;
    ft_default = init_fonts();
}

cairo_font_face_t * CircuitWidget::init_fonts()
{
    FT_Library library;
    FT_Face ft_face;
    FT_Init_FreeType( &library );
    FT_New_Face( library, "data/fonts/cmbx12.ttf", 0, &ft_face );
    return cairo_ft_font_face_create_for_ft_face (ft_face, 0);
}

void CircuitWidget::set_window (Gtk::Window *w)
{
    win = w;
}
void CircuitWidget::set_offset (int y)
{
    yoffset = y;
}

CircuitWidget::~CircuitWidget () {}
unsigned int CircuitWidget::getFirstWire (double my)
{
    unsigned int ans=0;
    double mindist=0;
    for (unsigned int i = 0; i < circuit->numLines (); i++) {
        double y = wireToY (i);
        if (i == 0 || mindist > abs(my-y)) {
            mindist = abs(my-y);
            ans = i;
        }
    }
    return ans;

}

void CircuitWidget::clear_selection ()
{
    selections.clear ();
    force_redraw ();
}

void CircuitWidget::on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time)
{
    (void)info; //placate compiler...
    (void)selection_data; //placate compiler...
    if (!circuit) {
        context->drag_finish(false, false, time);
        return;
    }
    selections.clear();
    ((QCViewer*)win)->set_selection (selections);
    Gtk::Widget* widget = drag_get_source_widget(context);
    Gtk::Button* button = dynamic_cast<Gtk::Button*>(widget);
    if (button == NULL) {
        context->drag_finish(false, false, time);
        return;
    }

    Gate *newgate = NULL;
    unsigned int target = 0;
    switch (((GateIcon*)button->get_image ())->type) {
    case GateIcon::NOT:
        newgate = new UGate ("X");
        newgate->drawType = Gate::NOT;
        break;
    case GateIcon::R:
        newgate = new RGate (1.0, RGate::Z);
        break;
    case GateIcon::SWAP:
        newgate = new UGate ("F");
        newgate->drawType = Gate::FRED;
        newgate->targets.push_back (target++);
        break;
    default:
        newgate = new UGate(((GateIcon*)button->get_image ())->symbol);
        break;
    }
    if (newgate->targets.size () > circuit->numLines ()) {
        delete newgate;
        context->drag_finish (false, false, time);
        return;
    }
    newgate->targets.push_back(target++);
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();
    // translate mouse click coords into circuit diagram coords
    double xx = (x - width/2.0 + ext.width/2.0)/scale + cx;// - cx*scale;
    double yy = (y - height/2.0 + ext.height/2.0)/scale + cy;// - cy*scale;
    vector<int> select_ids;
    pickRect (columns, xx, yy, select_ids);
    unsigned int wire = getFirstWire (yy);
    if (wire + newgate->targets.size () - 1 >= circuit->numLines ()) wire = circuit->numLines () - newgate->targets.size ();
    for (unsigned int i = 0; i < newgate->targets.size(); i++) newgate->targets[i] += wire;
    if (columns.empty()) {
        insert_gate_at_front (newgate);
    } else if (select_ids.size() == 0) {
        if (yy < columns[0].y0 || yy - columns[0].y0 > columns[0].height) {
            // bad drag and drop
        } else if (xx < columns[0].x0) {
            insert_gate_at_front (newgate);
        } else {
            unsigned int i;
            for (i = 1; i < columns.size (); i++) {
                if (xx < columns[i].x0) {
                    insert_gate_in_new_column (newgate, layout[i-1].lastGateID);
                    break;
                }
            }
            if (i == columns.size ()) { // goes after all columns
                insert_gate_in_new_column (newgate, layout[i-1].lastGateID);
            }
        }
    } else {
        unsigned int start = (select_ids.size()== 0) ? 0 : layout[select_ids.at(0) - 1].lastGateID + 1;
        unsigned int end   = layout[select_ids.at(0)].lastGateID;
        bool ok = true;
        unsigned int mymaxwire, myminwire;
        mymaxwire = myminwire = newgate->targets[0];
        for (unsigned int j = 0; j < newgate->targets.size (); j++) {
            mymaxwire = max (mymaxwire, newgate->targets[j]);
            myminwire = min (myminwire, newgate->targets[j]);
        }
        for (unsigned int i = start; i <= end && ok; i++) {
            unsigned int maxwire, minwire;
            Gate* g = circuit->getGate (i);
            minmaxWire (g->controls, g->targets, minwire, maxwire);
            if (!(mymaxwire < minwire || myminwire > maxwire)) ok = false;
        }
        if (ok) {
            insert_gate_in_column (newgate, select_ids.at(0));
        } else {
            insert_gate_in_new_column (newgate, end);
        }
    }
    context->drag_finish(true, false, time);
}

bool CircuitWidget::on_button_press_event (GdkEventButton* event)
{
    if (!circuit) return true;
    if (event->button == 1) {
        panning = true;
        oldmousex = event->x;
        oldmousey = event->y;
        select_rect.x0 = event->x;
        select_rect.y0 = event->y;
        select_rect.width = 0;
        select_rect.height = 0;
    }
    return true;
}

bool CircuitWidget::onMotionEvent (GdkEventMotion* event)
{
    if (!circuit) return true;
    if (panning) {
        cx -= (event->x - oldmousex)/scale;
        cy -= (event->y - oldmousey)/scale;
        oldmousex = event->x;
        oldmousey = event->y;
        force_redraw ();
    }
    return true;
}

bool CircuitWidget::on_button_release_event(GdkEventButton* event)
{
    if (!circuit) return true;
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();
    // translate mouse click coords into circuit diagram coords
    double x = (event->x - width/2.0 + ext.width/2.0)/scale + cx;// - cx*scale;
    double y = (event->y - height/2.0 + ext.height/2.0)/scale + cy;// - cy*scale;
    Gate* g;
    if (event->button == 3) {
        selections.clear ();
        ((QCViewer*)win)->set_selection (selections);
        force_redraw ();
    } else if (event->button == 1) {
        panning = false;
        int column_id = -1.0; // before column 0
        double mindist = -1.0;
        int wireid;
        double dist;
        unsigned int i;
        vector<Control>::iterator it;
        vector<unsigned int>::iterator it2;
        switch (mode) {
            break;
        case NORMAL: {
            gateRect r;
            r.x0 = (select_rect.x0-width/2.0+ext.width/2.0)/scale + cx;
            r.y0 = (select_rect.y0-height/2.0+ext.height/2.0)/scale + cy;
            r.width  = select_rect.width/scale;
            r.height = select_rect.height/scale;

            // make rect go in down/right direction
            r.x0 = min (r.x0, r.x0+r.width);
            r.y0 = min (r.y0, r.y0+r.height);
            r.width = abs(r.width);
            r.height = abs(r.height);

            if (!pickRects(rects, r).empty()) {
                selections.clear ();
                selections = pickRects (rects, r);
                ((QCViewer*)win)->set_selection (selections);
            }
            force_redraw ();
        }
        break;
        case EDIT_CONTROLS:
            if (selections.size () != 1) {
                cout << "very bad thing happened: " << __LINE__ << __FILE__ << endl;
                selections.clear ();
                ((QCViewer*)win)->set_selection (selections);
            }
            // find out which wire was clicked
            // get the control associated with this gate, wire
            // if null, add it as a positive control
            // if ctrl is positive, add as negative
            // if ctrl is negative. remove.
            if (rects[selections[0].gate].x0 > x || rects[selections[0].gate].x0+rects[selections[0].gate].width < x) return true;
            wireid = pickWire (y);
            if ((unsigned int)wireid >= circuit->numLines()) wireid = -1;
            if (wireid == -1) return true;
            g = circuit->getGate (selections[0].gate);
            for (it = g->controls.begin (); it != g->controls.end (); ++it) {
                if (it->wire == (unsigned int)wireid) {
                    it->polarity = !it->polarity;
                    if (!it->polarity) { // instead of cycling pos/neg, delete if it /was/ neg.
                        g->controls.erase (it);
                        force_redraw ();
                        return true;
                    }
                    force_redraw ();
                    return true;
                }
            }
            {
                // XXX: sick of using switch due to the declaration stuff. switch it to if-else later.
                vector<unsigned int>::iterator it = find (g->targets.begin (), g->targets.end (), wireid);
                if (it != g->targets.end ()) return true; // if a target, can't be a control.
                g->controls.push_back (Control(wireid, false));
                // now, calculate whether adding this control will make this gate overlap with another.
                unsigned int col_id;
                for (col_id = 0; col_id < layout.size () && selections[0].gate > layout[col_id].lastGateID; col_id++);
                unsigned int minW, maxW;
                minmaxWire (g->controls, g->targets, minW, maxW);
                unsigned int firstGateID = col_id == 0 ? 0 : layout[col_id - 1].lastGateID + 1;
                for (unsigned int i = firstGateID; i <= layout[col_id].lastGateID; i++) {
                    Gate* gg = circuit->getGate (i);
                    unsigned int minW2, maxW2;
                    minmaxWire (gg->controls, gg->targets, minW2, maxW2);
                    if (i != selections[0].gate && !(minW2 > maxW || maxW2 < minW)) {
                        circuit->swapGate (firstGateID, selections[0].gate); // pop it out to the left
                        selections[0].gate = firstGateID;
                        ((QCViewer*)win)->set_selection (selections);
                        layout.insert (layout.begin()+col_id, LayoutColumn (firstGateID));
                        force_redraw ();
                        return true;
                    }
                }
            }
            force_redraw ();
            return true;
            break;
        case EDIT_BREAKPOINTS:
            // note: the -1 on the range is so a breakpoint can't go after the last column. also can't go before first.
            for (i = 0; i < columns.size () - 1 && x >= columns[i].x0+columns[i].width; i++) {
                dist = abs(x - (columns[i].x0 + columns[i].width));
                if (dist < mindist || mindist == -1.0) {
                    mindist = dist;
                    column_id = i;
                }
            }
            if (column_id == -1) return true;
            it2 = find (breakpoints.begin (), breakpoints.end (), (unsigned int) column_id);
            if (it2 == breakpoints.end ()) {
                breakpoints.push_back ((unsigned int)column_id);
            } else {
                breakpoints.erase (it2);
            }
            force_redraw ();
            break;
        }
    }
    return true;
}
/* used to be able to select multiple gates. deemed silly. XXX: not so!
void CircuitWidget::toggle_selection (int id) {
  set <int>::iterator it;
  it = selections.find(id);
  if (it != selections.end()) selections.erase (it);
  else selections.insert (id);
  force_redraw ();
}*/

bool CircuitWidget::onScrollEvent (GdkEventScroll *event)
{
    double s;
    if (event->direction == 1) s = get_scale()/1.15;
    else s = get_scale()*1.15;
    set_scale(s);
    force_redraw ();
    return true;
}

bool CircuitWidget::on_expose_event(GdkEventExpose* event)
{
    (void)event; // placate compiler..
    Glib::RefPtr<Gdk::Window> window = get_window();
    if(window) {
        Gtk::Allocation allocation = get_allocation();
        const int width = allocation.get_width();
        const int height = allocation.get_height();
        double xc = width/2.0;
        double yc = height/2.0;

        Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
        cr->rectangle(event->area.x, event->area.y,
                      event->area.width, event->area.height);
        cr->clip();
        cr->rectangle (0, 0, width, height);
        cr->set_source_rgb (1,1,1);
        cr->fill ();
        cr->translate (xc-ext.width/2.0-cx*scale, yc-ext.height/2.0-cy*scale);
        if (circuit != NULL) {
            rects = circuit->draw (cr->cobj(), drawarch, drawparallel,  ext, wirestart, wireend, scale, selections, ft_default);
            generate_layout_rects ();
            for (unsigned int i = 0; i < NextGateToSimulate; i++) {
                drawRect (cr->cobj(), rects[i], Colour (0.1,0.7,0.2,0.7), Colour (0.1, 0.7,0.2,0.3));
            }
            for (unsigned int i = 0; i < breakpoints.size (); i++) {
                unsigned int j = breakpoints[i];
                double x = (columns[j].x0+columns[j].width+columns[j+1].x0)/2.0;
                double y = (0 - height/2.0 + ext.height/2.0)/scale + cy;

                cr->set_source_rgba (0.8,0,0,0.8);
                cr->move_to (x, y);
                cr->line_to (x, y+height/scale);
                cr->stroke ();
            }
        }
    }
    return true;
}

void CircuitWidget::newcircuit (unsigned int numqubits)
{
    if (circuit != NULL) delete circuit;
    circuit = new Circuit ();
    layout.clear ();
    breakpoints.clear ();
    for (unsigned int i = 0; i < numqubits; i++) {
        stringstream ss;
        ss << i + 1;
        circuit->addLine (ss.str ());
        circuit->getLineModify(i).outLabel = ss.str ();
        circuit->getLineModify(i).constant = false;
        circuit->getLineModify(i).garbage = false;
    }
}

vector<string> CircuitWidget::load (string file)
{
    if (circuit != NULL) delete circuit;
    vector<string> error_log;
    circuit = parseCircuit(file,error_log);
    layout.clear ();
    breakpoints.clear ();
    cx = cy = 0;
    if (circuit == NULL) {
        cout << "Error loading circuit" << endl;
        return error_log;
    }
    vector<unsigned int> parallels = circuit->getGreedyParallel ();

    for (unsigned int i = 0; i < parallels.size(); i++) {
        layout.push_back (LayoutColumn(parallels[i]));
    }
    return error_log;
}

void CircuitWidget::loadArch (string file)
{
    if (circuit) {
        circuit->parseArch (file);
        force_redraw ();
    }
}

void CircuitWidget::force_redraw ()
{
    Glib::RefPtr<Gdk::Window> win = get_window();
    if (win) {
        Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                         get_allocation().get_height());
        win->invalidate_rect(r, false);
    }
}

void CircuitWidget::set_drawarch (bool foo)
{
    drawarch = foo;
    force_redraw ();
}
void CircuitWidget::set_drawparallel (bool foo)
{
    drawparallel = foo;
    force_redraw ();
}

void CircuitWidget::save_circuit (string filename)
{
    saveCircuit(circuit,filename);
}

void CircuitWidget::savepng (string filename)
{
    if (!circuit) return;
    double wirestart, wireend;
    cairo_rectangle_t ext = circuit->get_circuit_size (&wirestart, &wireend, 1.0,ft_default);
    cairo_surface_t* surface = make_png_surface (ext);
    cairo_t* cr = cairo_create (surface);
    cairo_set_source_surface (cr, surface, 0, 0);
    circuit->draw( cr, drawarch, drawparallel,  ext, wirestart, wireend, 1.0, vector<Selection>(), ft_default);
    write_to_png (surface, filename);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

void CircuitWidget::savesvg (string filename)
{
    if (!circuit) return;
    double wirestart, wireend;
    cairo_rectangle_t ext = circuit->get_circuit_size (&wirestart, &wireend, 1.0, ft_default);
    cairo_surface_t* surface = make_svg_surface (filename, ext);
    cairo_t* cr = cairo_create (surface);
    cairo_set_source_surface (cr, surface, 0, 0);
    circuit->draw(cr, drawarch, drawparallel, ext, wirestart, wireend, 1.0, vector<Selection>(),ft_default);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

void CircuitWidget::saveps (string filename)
{
    if (!circuit) return;
    double wirestart, wireend;
    cairo_rectangle_t ext = circuit->get_circuit_size (&wirestart, &wireend, 1.0,ft_default);
    cairo_surface_t* surface = make_ps_surface (filename, ext);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_surface (cr, surface, 0,0);
    circuit->draw(cr, drawarch, drawparallel, ext, wirestart, wireend, 1.0, vector<Selection>(),ft_default);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

void CircuitWidget::set_scale (double x)
{
    if (!circuit) return;
    scale = x;
    ext = circuit->get_circuit_size (&wirestart, &wireend, scale, ft_default);
    force_redraw ();
}

// XXX: urg, may be duplicated elsewhere. check.
unsigned int findcolumn (vector<LayoutColumn>& layout, unsigned int gate)
{
    unsigned int i;
    for (i = 0; i < layout.size () && gate > layout[i].lastGateID; i++);
    return i - 1;
}

bool CircuitWidget::run (bool breaks)
{
    if (!circuit || !state) return false;
    bool ret = circuit->run(*state);
    force_redraw ();
    return ret;
}

bool CircuitWidget::step ()
{
    if (!circuit || !state) return false;
    bool ret = circuit->step(*state);
    force_redraw ();
    return ret;
}

void CircuitWidget::reset ()
{
    if(circuit!=NULL) {
        circuit->reset();
        NextGateToSimulate = 0;
        force_redraw ();
    }
}

void CircuitWidget::set_state (State* n_state)
{
    state = n_state;
}

double CircuitWidget::get_scale ()
{
    return scale;
}
int CircuitWidget::get_QCost ()
{
    if (circuit == NULL) return 0;
    return circuit->QCost();
}
int CircuitWidget::get_Depth ()
{
    if (circuit == NULL) return 0;
    return circuit->getParallel().size();
}
int CircuitWidget::get_NumGates ()
{
    if (circuit == NULL) return 0;
    return circuit->numGates();
}
unsigned int CircuitWidget::get_NumLines()
{
    if (circuit == NULL) return 0;
    return circuit->numLines();
}

void CircuitWidget::insert_gate_in_column (Gate *g, unsigned int column_id)
{
    for (unsigned int j = column_id; j < layout.size (); j++) layout[j].lastGateID += 1;
    circuit->addGate(g, layout[column_id].lastGateID - 1);
    circuit->getGreedyParallel();
    ext = circuit->get_circuit_size (&wirestart, &wireend, scale, ft_default);
    force_redraw ();
    selections.clear ();
    selections.push_back(layout[column_id].lastGateID - 1);
    ((QCViewer*)win)->set_selection (selections);
}

void CircuitWidget::insert_gate_at_front (Gate *g)
{
    for (unsigned int j = 0; j < layout.size (); j++) layout[j].lastGateID += 1;
    circuit->addGate(g, 0);
    circuit->getGreedyParallel();
    layout.insert(layout.begin(), LayoutColumn (0));
    ext = circuit->get_circuit_size (&wirestart, &wireend, scale, ft_default);
    force_redraw ();
    selections.clear ();
    selections.push_back(0);
    ((QCViewer*)win)->set_selection (selections);
}

// XXX: this may actually be more complicated than necessary now.
void CircuitWidget::insert_gate_in_new_column (Gate *g, unsigned int x)
{
    if (!circuit) return;
    unsigned int i;
    for (i = 0; i < layout.size() && layout[i].lastGateID < x; i++);
    unsigned int pos = layout[i].lastGateID + 1;
    for (unsigned int j = i + 1; j < layout.size (); j++) layout[j].lastGateID += 1;
    circuit->addGate (g, pos);
    circuit->getGreedyParallel();
    layout.insert (layout.begin() + i + 1, LayoutColumn (pos));
    ext = circuit->get_circuit_size (&wirestart, &wireend, scale, ft_default);
    force_redraw ();
    selections.clear ();
    selections.push_back (pos);
    ((QCViewer*)win)->set_selection (selections);
}

void CircuitWidget::delete_gate (unsigned int id)
{
    if (!circuit || id >= circuit->numGates()) return;
    unsigned int i = 0;
    selections.clear ();
    NextGateToSimulate = 0;
    for (i = 0; i < layout.size(); i++) {
        if (layout[i].lastGateID > id) break;
        if (layout[i].lastGateID < id) continue;
        // layout[i].lastGateID == id
        if (i == 0 && id != 0) break;
        if (i == 0 || layout[i - 1].lastGateID == id - 1) {
            vector<unsigned int>::iterator it = find (breakpoints.begin (), breakpoints.end (), i);
            if (it != breakpoints.end ()) breakpoints.erase (it);
            layout.erase (layout.begin() + i);
            break;
        } else break;
    }
    for (; i < layout.size (); i++) layout[i].lastGateID -= 1;
    circuit->removeGate (id);
    ext = circuit->get_circuit_size (&wirestart, &wireend, scale, ft_default);
    force_redraw ();
}

void CircuitWidget::generate_layout_rects ()
{
    delete_recs(columns);
    columns.clear ();
    if (!circuit || circuit->numGates () == 0) return;
    unsigned int start_gate = 0;
    for (unsigned int column = 0; column < layout.size() && start_gate < circuit->numGates (); column++) {
        gateRect bounds = rects.at(start_gate);
        for (unsigned int gate = start_gate + 1; gate <= layout[column].lastGateID ; gate++) {
            bounds = combine_gateRect(bounds, rects[gate]);
        }
        bounds.y0 = ext.y;
        bounds.height = max (bounds.height, ext.height);
        columns.push_back(bounds);
        start_gate = layout[column].lastGateID + 1;
    }
}

void CircuitWidget::set_mode (Mode m)
{
    mode = m;
}

Gate* CircuitWidget::getSelectedSubGate (Circuit* circuit, vector<Selection> *selections)
{
    if (!circuit || selections->size () != 1) {
        if (selections->size () > 1) cout << "bad: getSelectedGate when multiple gates selected.\n";
        return NULL;
    }
    Gate* g = circuit->getGate(selections->at(0).gate);
    if (selections->at(0).sub!=NULL && selections->at(0).sub->size() == 1 && g->type==Gate::SUBCIRC&& ((Subcircuit*)g)->expand&& !((Subcircuit*)g)->unroll ) {
        g = getSelectedSubGate(((Subcircuit*)g)->getCircuit(),selections->at(0).sub);
    }
    return g;
}

Gate *CircuitWidget::getSelectedGate ()
{
    if (!circuit || selections.size () != 1) {
        if (selections.size () > 1) cout << "bad: getSelectedGate when multiple gates selected.\n";
        return NULL;
    }
    Gate* g = circuit->getGate(selections.at(0).gate);
    if (selections.at(0).sub!=NULL && selections.at(0).sub->size() == 1 && g->type==Gate::SUBCIRC && ((Subcircuit*)g)->expand && !((Subcircuit*)g)->unroll ) {
        g = getSelectedSubGate(((Subcircuit*)g)->getCircuit(),selections.at(0).sub);
    }
    return g;
}

void CircuitWidget::deleteSelectedSubGate (Circuit* circuit, vector<Selection> *selections)
{
    if (!circuit || selections->size () != 1) {
        if (selections->size () > 1) cout << "bad: deleteSelectedGate when multiple gates selected.\n";
    }
    Gate* g = circuit->getGate(selections->at(0).gate);
    if (selections->at(0).sub!=NULL && selections->at(0).sub->size() == 1 && g->type==Gate::SUBCIRC&& ((Subcircuit*)g)->expand&& !((Subcircuit*)g)->unroll ) {
        g = getSelectedSubGate(((Subcircuit*)g)->getCircuit(),selections->at(0).sub);
    } else {
			circuit->removeGate(selections->at(0).gate);
		}

}
void CircuitWidget::deleteSelectedGate ()
{
    if (!circuit || selections.size () != 1) {
        if (selections.size () > 1) cout << "bad: deleteSelectedGate when multiple gates selected.\n";
    }
    Gate* g = circuit->getGate(selections.at(0).gate);
    if (selections.at(0).sub!=NULL && selections.at(0).sub->size() == 1 && g->type==Gate::SUBCIRC && ((Subcircuit*)g)->expand && !((Subcircuit*)g)->unroll ) {
        deleteSelectedSubGate(((Subcircuit*)g)->getCircuit(),selections.at(0).sub);
    } else {
			delete_gate(selections.at(0).gate);
		}
		force_redraw();
}




void  CircuitWidget::arch_set_LNN()
{
    circuit->arch_set_LNN();
}

void CircuitWidget::add_subcirc ()
{
    /*
    Loop l;
    l.first = *(std::min_element(selections.begin(), selections.end()));
    l.last = *(std::max_element(selections.begin(), selections.end()));
    l.n = 1;
    l.sim_n = l.n;
    l.label = "newloop";
    circuit->add_loop (l);
    */
}

bool CircuitWidget::is_subcirc (unsigned int id)
{
    return circuit->getGate(id)->type == Gate::SUBCIRC;
}

Gate* CircuitWidget::getGate(unsigned int id)
{
    return circuit->getGate(id);
}

void CircuitWidget::delete_recs(std::vector<gateRect>& recs)
{
    for (unsigned int i = 0; i < recs.size(); i++) {
        recs.at(i).remove();
    }
}
