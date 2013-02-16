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
#include <memory>
#include "GateIcon.h"
#include <cairomm/context.h>
#include "QCLib/circuit.h"
#include "QCLib/subcircuit.h"
#include "QCLib/circuitParser.h"
#include "QCLib/simulate.h"
#include <iostream>
#include <gtkmm.h>
#include "QCLib/gate.h"
#include "window.h" // slows down compiles, would be nice to not need this (see: clicking, effects toolpalette)

using namespace std;

CircuitWidget::CircuitWidget(DrawOptions drawOp) : circuit(std::shared_ptr<Circuit>()),circuitDrawer(drawOp)
{
    state = NULL;
    win = NULL;
    cx = cy = 0;
    size_changed = panning = drawarch = drawparallel = false;
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

void CircuitWidget::toggle_linelabels()
{
    circuitDrawer.toggleLineLabels();
    size_changed = true;
    force_redraw ();
}

CircuitWidget::~CircuitWidget () {}
unsigned int CircuitWidget::getFirstWire (double my)
{
    unsigned int ans=0;
    double mindist=0;
    for (unsigned int i = 0; i < circuit->numLines (); i++) {
        double y = circuitDrawer.wireToY (i);
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
    static_cast<QCViewer*>(win)->set_selection (selections);
    Gtk::Widget* widget = drag_get_source_widget(context);
    Gtk::Button* button = dynamic_cast<Gtk::Button*>(widget);
    if (button == NULL) {
        context->drag_finish(false, false, time);
        return;
    }

    shared_ptr<Gate> newgate;
    unsigned int target = 0;
    switch (static_cast<GateIcon*>(button->get_image())->type) {
    case GateIcon::NOT:
        newgate = shared_ptr<Gate>(new UGate ("X"));
        newgate->drawType = Gate::NOT;
        break;
    case GateIcon::R:
        newgate = shared_ptr<Gate>(new RGate (1.0, RGate::Z));
        break;
    case GateIcon::SWAP:
        newgate = shared_ptr<Gate>(new UGate ("F"));
        newgate->drawType = Gate::FRED;
        newgate->targets.push_back (target++);
        break;
    case GateIcon::MEASURE:
        newgate = shared_ptr<Gate>(new UGate("MEASURE",""));
        newgate->drawType = Gate::MEASURE;
        break;
    default:
        newgate = shared_ptr<Gate>(new UGate(static_cast<GateIcon*>(button->get_image ())->symbol));
        break;
    }
    if (newgate->targets.size () > circuit->numLines ()) {
        context->drag_finish (false, false, time);
        return;
    }
    newgate->targets.push_back(target++);
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();
    // translate mouse click coords into circuit diagram coords
    double xx = (x - width/2.0 + ext.width/2.0)/scale + cx;
    double yy = (y - height/2.0 + ext.height/2.0)/scale + cy;
    vector<int> select_ids;
    string name;
    int pos = -1;
    getCircuitAndColPosition (xx, yy, circuit, rects, name, pos);
    if (name.compare("Main")==0||pos==-1||name.compare("")==0) {  //If the click is not in a subcircuit
        vector<unsigned int> para =  circuit->getGreedyParallel();
        unsigned int wire = getFirstWire (yy);
        if (wire + newgate->targets.size () - 1 >= circuit->numLines ()) {
            wire = circuit->numLines () - newgate->targets.size ();
        }
        for (unsigned int i = 0; i < newgate->targets.size(); i++) {
            newgate->targets[i] += wire;
        }
        if (columns.empty()||pos==-1) {
            insert_gate_at_front (newgate);
        } else if (pos >= (int)para.size()) {
            insert_gate_in_new_column (newgate, circuit->numGates(),circuit);
        } else {
            insert_gate_in_new_column (newgate, para.at(pos),circuit);
        }
        circuit->getGreedyParallel();
    } else {
        insert_gate_in_new_column (newgate, pos,circuit->subcircuits[name]);
        unsigned int wire = getFirstWire (yy);
        for (unsigned int i = 0; i < newgate->targets.size(); i++) newgate->targets[i] += wire;
    }
    context->drag_finish(true, false, time);
}

bool CircuitWidget::on_button_press_event (GdkEventButton* event)
{
    if (!circuit) {
        return true;
    } else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
        // Double-click.
        panning = false;
        if(circuitDrawer.usingLineLabels()) {
            // If a line label is double-clicked, edit it.
            Gtk::Allocation allocation = get_allocation();
            const int width = allocation.get_width();
            const int height = allocation.get_height();
            // translate mouse click coords into circuit diagram coords
            double x = (event->x - width/2.0 + ext.width/2.0)/scale + cx;// - cx*scale;
            double y = (event->y - height/2.0 + ext.height/2.0)/scale + cy;// - cy*scale;
            vector<int> selections;
            int res = pickRect(wirelabels, x, y, selections);
            if(res>=0) {
                edit_line_label(res);
            }
        }
        return true;
    } else if (event->button == 1) {
        panning = true;
        oldmousex = event->x;
        oldmousey = event->y;
        select_rect.x0 = event->x;
        select_rect.y0 = event->y;
        select_rect.width = 0;
        select_rect.height = 0;
        return true;
    } else {
        return true;
    }
}

void CircuitWidget::edit_line_label (uint32_t line)
{
    if(!circuit) {
        return;
    }
    Gtk::Dialog enterLabel("Enter Label");
    enterLabel.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    enterLabel.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    enterLabel.set_default_response(Gtk::RESPONSE_OK);
    Gtk::Entry labelEntry;
    labelEntry.set_activates_default();
    labelEntry.set_max_length(5000);
    labelEntry.show();
    enterLabel.get_vbox()->pack_start(labelEntry,Gtk::PACK_SHRINK);
    int result = enterLabel.run();
    if (result == Gtk::RESPONSE_OK
            && std::string(labelEntry.get_text()) != "") {
        if(line >= circuit->numLines()) {
            Line & l = circuit->getLineModify(line - circuit->numLines());
            l.outLabel = labelEntry.get_text();
        } else {
            Line & l = circuit->getLineModify(line);
            l.lineName = labelEntry.get_text();
            l.constant = false;
        }
        size_changed = true;
        force_redraw();
    }
}

bool CircuitWidget::onMotionEvent (GdkEventMotion* event)
{
    if (!circuit) {
        return true;
    } else if (panning) {
        cx -= (event->x - oldmousex)/scale;
        cy -= (event->y - oldmousey)/scale;
        oldmousex = event->x;
        oldmousey = event->y;
        force_redraw ();
        return true;
    } else {
        return true;
    }
}

void CircuitWidget::check_circuit_size()
{
    if (circuit&&size_changed) {
        ext = circuitDrawer.getCircuitSize (*circuit, wirestart, wireend, scale, ft_default);
        size_changed = false;
    }
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
    shared_ptr<Gate> g;
    if (event->button == 3) {
        selections.clear ();
        static_cast<QCViewer*>(win)->set_selection (selections);
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
                static_cast<QCViewer*>(win)->set_selection (selections);
            }
            force_redraw ();
        }
        break;
        case EDIT_CONTROLS:
            if (selections.size () != 1) {
                cout << "very bad thing happened: " << __LINE__ << __FILE__ << endl;
                selections.clear ();
                static_cast<QCViewer*>(win)->set_selection (selections);
            }
            // find out which wire was clicked
            // get the control associated with this gate, wire
            // if null, add it as a positive control
            // if ctrl is positive, add as negative
            // if ctrl is negative. remove.
            if (rects[selections[0].gate].x0 > x || rects[selections[0].gate].x0+rects[selections[0].gate].width < x) {
                return true;
            }
            wireid = circuitDrawer.pickWire(y);
            if ((unsigned int)wireid >= circuit->numLines()) wireid = -1;
            if (wireid == -1) return true;
            g = circuit->getGate (selections[0].gate);
            for (it = g->controls.begin (); it != g->controls.end (); ++it) {
                if (it->wire == (unsigned int)wireid) {
                    it->polarity = !it->polarity;
                    if (!it->polarity) { // instead of cycling pos/neg, delete if it /was/ neg.
                        g->controls.erase (it);
                        circuit->getGreedyParallel();
                        force_redraw ();
                        return true;
                    }
                    circuit->getGreedyParallel();
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
                for (col_id = 0; col_id < circuit->columns.size () && selections[0].gate > circuit->columns.at(col_id); col_id++);
                unsigned int minW, maxW;
                minmaxWire (g->controls, g->targets, minW, maxW);
                unsigned int firstGateID = col_id == 0 ? 0 : circuit->columns.at(col_id - 1) + 1;
                for (unsigned int i = firstGateID; i <= circuit->columns.at(col_id); i++) {
                    shared_ptr<Gate> gg = circuit->getGate (i);
                    unsigned int minW2, maxW2;
                    minmaxWire (gg->controls, gg->targets, minW2, maxW2);
                    if (i != selections[0].gate && !(minW2 > maxW || maxW2 < minW)) {
                        circuit->swapGate (firstGateID, selections[0].gate); // pop it out to the left
                        selections[0].gate = firstGateID;
                        static_cast<QCViewer*>(win)->set_selection (selections);
                        circuit->getGreedyParallel();
                        force_redraw ();
                        return true;
                    }
                }
            }
            circuit->getGreedyParallel();
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
            string name;
            int pos = -1;
            getCircuitAndColPosition (x, y, circuit, rects, name, pos);
            if (column_id != -1) {
                if (name.compare("Main")==0||pos==-1||name.compare("")==0) {  //If the click is not in a subcircuit
                    circuit->getGate(circuit->columns.at(column_id))->breakpoint =!circuit->getGate(circuit->columns.at(column_id))->breakpoint ;
                } else {
                    std::shared_ptr<Circuit> c = circuit->subcircuits[name];
                    vector<unsigned int> para =  c->getGreedyParallel();
                    c->getGate(para.at(pos))->breakpoint = !c->getGate(para.at(pos))->breakpoint;
                }
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
        circuitDrawer.renderCairo(cr->cobj());
        cr->rectangle(event->area.x, event->area.y,
                      event->area.width, event->area.height);
        cr->clip();
        cr->rectangle (0, 0, width, height);
        cr->set_source_rgb (1,1,1);
        cr->fill ();
        cr->translate (xc-ext.width/2.0-cx*scale, yc-ext.height/2.0-cy*scale);
        if (circuit) {
            rects = circuitDrawer.draw(*circuit, drawarch, drawparallel, ext, wirestart, wireend, scale, selections, ft_default, wirelabels);
            generate_layout_rects ();
        }
    }

    return true;
}

void CircuitWidget::newcircuit (unsigned int numqubits)
{
    circuit = shared_ptr<Circuit>(new Circuit ());
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
    vector<string> error_log;
    circuit = parseCircuit(file,error_log);
    breakpoints.clear ();
    cx = cy = 0;
    if (!circuit) {
        cout << "Error loading circuit" << endl;
        return error_log;
    }
    circuit->getGreedyParallel ();
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
    check_circuit_size();
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
    circuitDrawer.savepng (*circuit, filename,ft_default);
}

void CircuitWidget::savesvg (string filename)
{
    if (!circuit) return;
    circuitDrawer.savesvg (*circuit, filename,ft_default);
}

void CircuitWidget::saveps (string filename)
{
    if (!circuit) return;
    circuitDrawer.saveps (*circuit, filename,ft_default);
}

void CircuitWidget::set_scale (double x)
{
    if (!circuit) return;
    scale = x;
    size_changed = true;
    force_redraw ();
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
    if(circuit) {
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
    if (!circuit) return 0;
    return circuit->QCost();
}
int CircuitWidget::get_Depth ()
{
    if (!circuit) return 0;
    return circuit->getParallel().size();
}
int CircuitWidget::get_num_gates ()
{
    if (!circuit) return 0;
    return circuit->totalGates();
}

int CircuitWidget::get_gate_count (string gname)
{
    if (!circuit) return 0;
    return circuit->gateCount(gname);
}

unsigned int CircuitWidget::get_depth()
{
    if (!circuit) return 0;
    return circuit->depth();
}

unsigned int CircuitWidget::get_num_lines()
{
    if (!circuit) return 0;
    return circuit->numLines();
}

void CircuitWidget::insert_gate_in_column (shared_ptr<Gate> g, unsigned int column_id)
{

    circuit->addGate(g, column_id);
    circuit->getGreedyParallel();
    size_changed = true;
    selections.clear ();
    selections.push_back(column_id);
    static_cast<QCViewer*>(win)->set_selection (selections);
    force_redraw ();
}

void CircuitWidget::insert_gate_at_front (shared_ptr<Gate> g)
{
    circuit->addGate(g, 0);
    circuit->getGreedyParallel();
    size_changed = true;
    selections.clear ();
    selections.push_back(0);
    static_cast<QCViewer*>(win)->set_selection (selections);
    force_redraw ();
}

void CircuitWidget::insert_gate_in_new_column (shared_ptr<Gate> g, unsigned int x, std::shared_ptr<Circuit> circ)
{
    if (!circuit) return;
    circ->addGate (g, x);
    circ->getGreedyParallel();
    size_changed = true;
    selections.clear ();
    force_redraw ();
    selections.push_back(x);
}

void CircuitWidget::generate_layout_rects ()
{
    columns.clear ();
    if (!circuit || circuit->numGates () == 0) return;
    unsigned int start_gate = 0;
    for (unsigned int column = 0; column < circuit->columns.size() && start_gate < circuit->numGates (); column++) {
        gateRect bounds = rects.at(start_gate);
        for (unsigned int gate = start_gate + 1; gate <= circuit->columns.at(column) ; gate++) {
            bounds += rects[gate];
        }
        bounds.y0 = ext.y;
        bounds.height = max (bounds.height, ext.height);
        columns.push_back(bounds);
        start_gate = circuit->columns.at(column) + 1;
    }
}

void CircuitWidget::set_mode (Mode m)
{
    mode = m;
}

shared_ptr<Gate> CircuitWidget::getSelectedSubGate (shared_ptr<Circuit> circuit, vector<Selection> selections)
{
    if (!circuit || selections.size () != 1) {
        if (selections.size () > 1) cout << "bad: getSelectedGate when multiple gates selected.\n";
        return shared_ptr<Gate>();
    }
    shared_ptr<Gate> g = circuit->getGate(selections.at(0).gate);
    shared_ptr<Subcircuit> s = dynamic_pointer_cast<Subcircuit>(g);
    if ( s ) {
        if (!selections.at(0).sub.empty() && selections.at(0).sub.size() == 1 && s->expand ) {
            if (s->unroll) {
                selections.at(0).sub.at(0).gate = selections.at(0).sub.at(0).gate % s->numGates();
                g = getSelectedSubGate(s->getCircuit(),selections.at(0).sub);
            } else {
                g = getSelectedSubGate(s->getCircuit(),selections.at(0).sub);
            }
        }
    }
    return g;
}

shared_ptr<Gate> CircuitWidget::getSelectedGate ()
{
    shared_ptr<Gate> g = getSelectedSubGate(circuit,selections);
    return g;
}

void CircuitWidget::deleteSelectedSubGate (std::shared_ptr<Circuit> circuit, vector<Selection> selections)
{
    if (circuit && selections.size () > 0 && selections.at(0).gate < circuit->numGates()) {
        shared_ptr<Gate> g = circuit->getGate(selections.at(0).gate);
        shared_ptr<Subcircuit> s = dynamic_pointer_cast<Subcircuit>(g);
        if (s && !selections.at(0).sub.empty() && selections.at(0).sub.size() == 1 && s->expand && !s->unroll) {
            deleteSelectedSubGate(s->getCircuit(),selections.at(0).sub);
        } else {
            circuit->removeGate(selections.at(0).gate);
        }
    }
}

void CircuitWidget::deleteSelectedGate ()
{
    deleteSelectedSubGate (circuit, selections);
    force_redraw();
}

void  CircuitWidget::expand_all()
{
    circuit->expandAll();
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
    if (dynamic_pointer_cast<Subcircuit>(circuit->getGate(id))) {
        return true;
    } else {
        return false;
    }
}

shared_ptr<Gate> CircuitWidget::getGate(unsigned int id)
{
    return circuit->getGate(id);
}

void CircuitWidget::getCircuitAndColPosition (double x, double y, std::shared_ptr<Circuit> c, vector<gateRect> &rects, string &r_name, int &r_pos)
{
    vector<int> s;
    int select = pickRect(rects,x,y,s);
    if (c->numGates()!=0 && select >= (int)c->numGates()) {
        select = select % c->numGates();
    }
    shared_ptr<Subcircuit> sub;
    if (select!=-1) {
        shared_ptr<Subcircuit> sub = dynamic_pointer_cast<Subcircuit>(c->getGate(select));
    }
    if (sub && sub->expand && !rects.at(select).subRects.empty()) {
        getCircuitAndColPosition (x, y, sub->getCircuit(), rects.at(select).subRects, r_name, r_pos );
    } else {
        vector<unsigned int> para = c->getGreedyParallel();
        vector<gateRect> cols;
        unsigned int start_gate = 0;
        for (unsigned int column = 0; column < para.size() && start_gate < c->numGates(); column++) {
            gateRect bounds = rects.at(start_gate);
            for (unsigned int gate = start_gate + 1; gate <= para.at(column); gate++) {
                bounds += rects.at(gate);
            }
            bounds.y0 = ext.y;
            bounds.height = max (bounds.height, ext.height);
            cols.push_back(bounds);
            start_gate = para.at(column) + 1;
        }
        if (x < 0) {
            r_pos = -1;
        } else {
            for (unsigned int i = 0; i < cols.size (); i++) {
                if (x < cols.at(i).x0) {
                    r_pos = i;
                    break;
                }
                if (i == cols.size()-1) r_pos = i+1;
            }
        }
        r_name = c->getName();
    }
}
