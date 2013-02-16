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


#include <assert.h>
#include <iostream>
#include <string>
#include "window.h"
#include "info.h"
#include <gtkmm/stock.h>
#include "QCLib/gates/UGateLookup.h"
#include "QCLib/state.h"
#include "QCLib/dirac.h"
#include "QCLib/utility.h"
#include "QCLib/subcircuit.h"
#include "QCLib/text.h"

/* Fix bug due to old GDK on Debian <= 6.0.6 */
#ifndef GDK_KEY_Delete
#define GDK_KEY_Delete GDK_Delete
#endif

using namespace std;
void QCViewer::setup_gate_button (Gtk::Button* btn, GateIcon *g)
{
    btn->set_image (*g);
    btn->drag_source_set (listTargets);
    btn->signal_drag_data_get().connect(sigc::mem_fun(*this, &QCViewer::dummy));
}

void QCViewer::dummy(const Glib::RefPtr<Gdk::DragContext>&, Gtk::SelectionData& selection_data, guint, guint)
{
    selection_data.set(selection_data.get_target(), 8 /* 8 bits format */,
                       (const guchar*)"I'm Data!",
                       9 /* the length of I'm Data! in bytes */);
}

QCViewer::QCViewer(QCVOptions ops) : c(ops.draw), options(ops)
{
    add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK );
    std::cerr << "In QCViewer::QCViewer.\n";
    drawparallel = drawarch = false;
    set_title(QCV_NAME);
    set_border_width(0);
    set_default_size(1000,1000);
    state = NULL;

    add(m_vbox);
    m_vbox.pack_end(m_statusbar,Gtk::PACK_SHRINK);

    register_stock_items();
    setup_menu_actions();
    setup_menu_layout();
    setup_gate_icons();

    if (!m_SimulateToolbar) {
        cout << "warning failed to create toolbar" << endl;
        return;
    }
    m_vbox.pack_start(*m_SimulateToolbar, Gtk::PACK_SHRINK);
    m_SimulateToolbar->show ();
    c.set_window (this);
    c.show();
    m_VisBox.set_homogeneous ();
    m_hbox.pack_end (c);
    m_hbox.pack_start (m_EditSidebar, Gtk::PACK_SHRINK);
    m_EditVisPane.pack1 (m_hbox, true, true);
    m_EditVisPane.pack2 (m_VisBox, true, true);
    m_vbox.pack_start (m_EditVisPane);

    m_vbox.show();
    m_hbox.show();
    m_VisBox.show ();
    show_all_children ();
    m_PropFrame.hide ();
    m_FlowFrame.hide();
    m_Subcirc.hide ();
    m_RGateEditFrame.hide ();
    std::cerr << "Done QCViewer::QCViewer\n";
}

QCViewer::~QCViewer() {}

bool QCViewer::on_key_release_event(GdkEventKey* event)
{
    cout << event->keyval << ":" << GDK_KEY_Delete << endl;
    if (event->keyval == GDK_KEY_Delete) {
        on_menu_delete ();
    }
    return true;
}

void QCViewer::unimplemented ()
{
    Gtk::MessageDialog dialog(*this, "Feature Unimplemented");
    dialog.set_secondary_text(
        "This feature doesn't exist yet/is currently disabled.");
    dialog.run();
}

void QCViewer::set_raxis ()
{
    shared_ptr<Gate> g = c.getSelectedGate ();
    shared_ptr<RGate> rg = dynamic_pointer_cast<RGate>(g);
    if (!rg) {
        cout << "UNEXPECTED THING HAPPENED!!!! " << __FILE__ << __LINE__ << endl;
    } else  {
        RGate::Axis na;
        if (btn_RX.get_active ()) {
            na = RGate::X;
        } else if (btn_RY.get_active ()) {
            na = RGate::Y;
        } else  {
            na = RGate::Z;
        }
        if (na != rg->get_axis ()) {
            rg->set_axis (na);
            c.force_redraw ();
        }
    }
}

void QCViewer::set_rval ()
{
    shared_ptr<Gate> g = c.getSelectedGate ();
    shared_ptr<RGate> rg = dynamic_pointer_cast<RGate>(g);
    if (!rg) {
        cout << "UNEXPECTED THING HAPPENED!!!! " << __FILE__ << __LINE__ << endl;
    } else {
        istringstream ss (m_RValEntry.get_text ());
        float nr;
        ss >> nr;
        if (ss.fail ()) {
            stringstream ss;
            ss << rg->get_rotVal ();
            m_RValEntry.set_text (ss.str ());
            Gtk::MessageDialog dialog(*this, "Error");
            dialog.set_secondary_text("Rotation factor must be a floating point number.");
            dialog.run();
            return;
        }
        rg->set_rotVal (nr);
        c.force_redraw ();
    }
}

extern TextEngine textEngine;
void QCViewer::on_menu_textmode_regular()
{
    textEngine.setMode(TEXT_PANGO);
    c.force_redraw();
}
void QCViewer::on_menu_textmode_latex()
{
    textEngine.setMode(TEXT_LATEX);
    /* XXX hack: circuit needs to render once in !forreal mode
       to batch latex calls. */
    c.set_scale(c.get_scale());
}

void QCViewer::on_menu_about ()
{
    vector<Glib::ustring> authors;
    authors.push_back("Alex Parent <aparent@uwaterloo.ca>");
    authors.push_back("Jacob Parker <j3parker@uwaterloo.ca>");
    authors.push_back("Marc Burns <m4burns@uwaterloo.ca>");
    Gtk::AboutDialog dialog;
    dialog.set_version(QCV_VERSION);
    dialog.set_program_name(QCV_NAME);
    dialog.set_website(QCV_WEBSITE);
    dialog.set_authors(authors);
    dialog.run();
}

void QCViewer::on_menu_file_open_circuit ()
{
    Gtk::FileChooserDialog dialog("Please choose a circuit file",
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
    Gtk::FileFilter qc_filter;
    qc_filter.set_name("QC files");
    qc_filter.add_pattern("*.qc");
    Gtk::FileFilter tfc_filter;
    tfc_filter.set_name("TFC files");
    tfc_filter.add_pattern("*.tfc");
    Gtk::FileFilter all_filter;
    all_filter.set_name("All files");
    all_filter.add_pattern("*");

    dialog.add_filter(all_filter);
    dialog.add_filter(qc_filter);
    dialog.add_filter(tfc_filter);
    dialog.set_filter(qc_filter);

    int result = dialog.run();
    if (result == Gtk::RESPONSE_OK) {
        open_circuit(dialog.get_filename ());
    }
}

void QCViewer::open_circuit(const std::string& filename)
{
    vector<string> errors = c.load (filename);
    if (!errors.empty()) {
        string error_message;
        for ( unsigned int i = 0; i < errors.size(); i++) {
            error_message += errors.at(i) + "\n";
        }
        Gtk::MessageDialog dialog(*this, "Error");
        dialog.set_secondary_text(error_message);
        dialog.run();
    }
    selections.clear ();
    c.set_drawparallel (drawparallel);
    c.set_drawarch (drawarch);
    c.set_scale (1);
    btn_editcontrols.set_active (false);
    btn_editcontrols.set_active (false);
    std::stringstream ss;
    ss << "Gates: " << c.get_num_gates() <<" | Depth: "<< c.get_depth() <<" | T-Count: " << c.get_gate_count ("T") << " | Qbits: " << c.get_num_lines();
    m_statusbar.push(ss.str());
    c.reset ();
}

void QCViewer::on_menu_file_open_arch ()
{
    Gtk::FileChooserDialog dialog ("Please choose an architecture file",
                                   Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for (*this);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
    int result = dialog.run ();
    if (result == Gtk::RESPONSE_OK) {
        c.loadArch (dialog.get_filename ());
    }
}

void QCViewer::on_menu_simulate_show_stateView()
{
    if (viz.size() < 3) {
        StateViewWidget* sw = new StateViewWidget (&m_statusbar, &m_VisBox, &viz, &m_EditVisPane);
        sw->set_state (state);
        m_VisBox.add (*sw);
        viz.push_back (sw);
        sw->show ();
        if (viz.size() == 1) {
            int w, h;
            get_size (w, h);
            m_EditVisPane.set_position (h - 400);
        }
    }
}

void QCViewer::on_menu_file_quit ()
{
    hide ();
}

void QCViewer::on_menu_save_png ()
{
    Gtk::FileChooserDialog dialog ("Please choose a png file to save to",
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for (*this);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    int result = dialog.run ();
    if (result == Gtk::RESPONSE_OK) {
        c.savepng (dialog.get_filename ());
    }
}

void QCViewer::on_menu_save_ps ()
{
    Gtk::FileChooserDialog dialog ("Please choose an eps file to save to",
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for (*this);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    int result = dialog.run ();
    if (result == Gtk::RESPONSE_OK) {
        c.saveps (dialog.get_filename ());
    }
}
void QCViewer::on_menu_save_circuit ()
{
    Gtk::FileChooserDialog dialog ("Please choose a qc file to save to",
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for (*this);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    int result = dialog.run ();
    if (result == Gtk::RESPONSE_OK) {
        c.save_circuit (dialog.get_filename ());
    }
}
void QCViewer::on_menu_save_svg ()
{
    Gtk::FileChooserDialog dialog ("Please choose a svg file to save to",
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for (*this);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    int result = dialog.run ();
    if (result == Gtk::RESPONSE_OK) {
        c.savesvg (dialog.get_filename ());
    }
}

void QCViewer::on_menu_options_parallel ()
{
    drawparallel = !drawparallel;
    c.set_drawparallel (drawparallel);
}

void QCViewer::on_menu_options_arch ()
{
    drawarch = !drawarch;
    c.set_drawarch (drawarch);
}

void QCViewer::on_menu_options_toggle_lab()
{
    c.toggle_linelabels();
}

void QCViewer::on_menu_set_arch_LNN()
{
    c.arch_set_LNN();
}

void QCViewer::on_menu_zoom_in ()
{
    double scale = c.get_scale ();
    c.set_scale (scale*1.125);
}

void QCViewer::on_menu_zoom_out ()
{
    double scale = c.get_scale ();
    c.set_scale (scale/1.125);
}

void QCViewer::on_menu_zoom_100 ()
{
    c.set_scale (1);
}

void QCViewer::on_menu_new ()
{
    Gtk::Dialog newdlg ("Number of qubits");
    newdlg.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    newdlg.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    newdlg.set_default_response(Gtk::RESPONSE_OK);
    Gtk::Entry num_qubits;
    num_qubits.set_activates_default();
    num_qubits.set_max_length (4);
    num_qubits.show();
    newdlg.get_vbox()->pack_start(num_qubits,Gtk::PACK_SHRINK);
    int result = newdlg.run();
    if (result == Gtk::RESPONSE_OK) {
        istringstream ss(num_qubits.get_text());
        unsigned int n = 0;
        ss >> n;
        if (ss.fail ()) {
            return;
        }
        selections.clear ();
        c.clear_selection ();
        c.newcircuit (n);
        c.set_drawparallel (drawparallel);
        c.set_drawarch (drawarch);
        c.set_scale (1);
        btn_editcontrols.set_active (false);
        btn_editcontrols.set_active (false);
        c.reset ();
    }
}
void QCViewer::on_menu_load_state ()
{
    Gtk::Dialog enterState("Enter State");
    enterState.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    enterState.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    enterState.set_default_response(Gtk::RESPONSE_OK);
    Gtk::Entry stateEntry;
    stateEntry.set_activates_default();
    stateEntry.set_max_length(5000);
    stateEntry.show();
    enterState.get_vbox()->pack_start(stateEntry,Gtk::PACK_SHRINK);
    int result = enterState.run();
    if (result == Gtk::RESPONSE_OK) {
        if (state!=NULL) delete state;
        state = getStateVec (stateEntry.get_text(), true);
        if (state!=NULL && state->numBits()!=0) {
            if (state->numBits() == c.get_num_lines()) {
                for (unsigned int i = 0; i < viz.size(); i++) viz[i]->set_state(state);
                c.set_state(state);
            } else {
                Gtk::MessageDialog dialog(*this, "Incorrect Dimension", false , Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
                dialog.set_secondary_text( "The dimension of the state you input does not match the number of lines in the circuit");
                dialog.run();
            }
        } else {
            Gtk::MessageDialog dialog(*this, "Syntax Error", false , Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
            dialog.run();
        }
    }
    c.reset ();
}

void QCViewer::load_state (State* s)
{
    state = s;
    for (unsigned int i = 0; i < viz.size(); i++) viz[i]->set_state(state);
    c.set_state(state);
    c.reset ();
}

void QCViewer::on_menu_step ()
{
    c.step();
    for (unsigned int i = 0; i < viz.size(); i++) viz[i]->reset ();
}

void QCViewer::on_menu_reset ()
{
    c.reset ();
}

void QCViewer::on_menu_run ()
{
    c.run (true);
    for (unsigned int i = 0; i < viz.size(); i++)  viz[i]->reset ();
}

void QCViewer::on_menu_delete ()
{
    c.deleteSelectedGate();
    set_selection (vector<Selection>());
}

void QCViewer::set_selection (vector<Selection> s)
{
    selections = s;
    if (selections.empty()) {
        btn_editcontrols.set_active (false);
        m_PropFrame.hide ();
        m_FlowFrame.hide();
        m_RGateEditFrame.hide ();
    } else if (selections.size () == 1) {
        shared_ptr<Gate> gate = c.getSelectedGate();
        shared_ptr<RGate> rg = dynamic_pointer_cast<RGate>(gate);
        if (rg) {
            m_RGateEditFrame.show ();
            switch (rg->get_axis ()) {
            case RGate::X:
                btn_RX.set_active ();
                break;
            case RGate::Y:
                btn_RY.set_active ();
                break;
            case RGate::Z:
                btn_RZ.set_active ();
                break;
            }
            stringstream ss;
            ss << rg->get_rotVal ();
            m_RValEntry.set_text (ss.str ());
        } else {
            m_RGateEditFrame.hide ();
        }
        m_IterEntry.set_text(intToString(c.getGate(selections[0].gate)->getLoopCount()));
        m_PropFrame.show ();
    } else {
        m_PropFrame.hide ();
        m_RGateEditFrame.show ();
    }
    // find out if we are in a loop
    if (!selections.empty()) {
        if (selections.size()>1) {
            //m_FlowFrame.show();
            m_Subcirc.hide();
        } else if (c.is_subcirc(selections[0].gate)) {
            m_Subcirc.show();
            m_FlowFrame.hide();
            m_SubcircNameEntry.set_text(c.getGate(selections[0].gate)->getName());
        } else {
            m_Subcirc.hide();
        }
    } else {
        m_Subcirc.hide();
        m_FlowFrame.hide();
    }

    btn_editbreakpoints.set_active (false);
}

/*
void QCViewer::add_loop ()
{
    c.add_loop ();
    set_selection (vector<uint32_t>());
    m_FlowFrame.hide();
    c.force_redraw();
}
*/

void QCViewer::set_loop_iter ()
{
    shared_ptr<Gate> g = c.getSelectedGate();
    g->setLoopCount(atoi(m_IterEntry.get_text().c_str()));
}

void QCViewer::set_subcircuit_name()
{
    shared_ptr<Gate> g = c.getSelectedGate();
    shared_ptr<Subcircuit> sub = dynamic_pointer_cast<Subcircuit>(g);
    if (sub) {
        sub->setName(m_SubcircNameEntry.get_text());
        c.force_redraw();
    }
}


void QCViewer::expand_subcirc()
{
    shared_ptr<Gate> g = c.getSelectedGate();
    shared_ptr<Subcircuit> sub = dynamic_pointer_cast<Subcircuit>(g);
    if (sub) {
        sub->expand = !sub->expand;
        c.force_redraw();
    }
}

void QCViewer::expand_all_subcirc()
{
    c.expand_all();
}

void QCViewer::unroll_subcirc()
{
    shared_ptr<Gate> g = c.getSelectedGate();
    shared_ptr<Subcircuit> sub = dynamic_pointer_cast<Subcircuit>(g);
    if (sub) {
        sub->unroll = !sub->unroll;
        c.force_redraw();
    }
}

void QCViewer::update_mode ()
{
    if (btn_editcontrols.get_active ()) {
        btn_editbreakpoints.set_active (false); // enforce di/trichotomy
        if (selections.size () != 1) {
            cout << "warning: this shouldn't have happened " << __FILE__ << " " << __LINE__ << endl;
            btn_editcontrols.set_active (false);
            c.set_mode (CircuitWidget::NORMAL);
            get_window ()->set_cursor ();
            return;
        } else {
            c.set_mode (CircuitWidget::EDIT_CONTROLS);
            get_window ()->set_cursor (Gdk::Cursor (Gdk::DOT));
        }
    } else if (btn_editbreakpoints.get_active ()) {
        get_window ()->set_cursor (Gdk::Cursor (Gdk::RIGHT_TEE));
        c.set_mode (CircuitWidget::EDIT_BREAKPOINTS);
    } else {
        get_window ()->set_cursor ();
        c.set_mode (CircuitWidget::NORMAL);
    }
}

void QCViewer::setup_menu_actions()
{
    m_refActionGroup = Gtk::ActionGroup::create();
    m_refActionGroup->add(Gtk::Action::create("File", "File"));
    m_refActionGroup->add(Gtk::Action::create("Circuit", "Circuit"));
    m_refActionGroup->add(Gtk::Action::create("Arch", "Architecture"));
    m_refActionGroup->add(Gtk::Action::create("Diagram", "Diagram"));

    m_refActionGroup->add(Gtk::Action::create("CircuitNew", Gtk::Stock::NEW, "New", "Create new circuit"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_new));
    m_refActionGroup->add(Gtk::Action::create("ArchNew", Gtk::Stock::NEW, "New", "Create new architecture"),
                          sigc::mem_fun(*this, &QCViewer::unimplemented));

    m_refActionGroup->add(Gtk::Action::create("CircuitOpen", Gtk::Stock::OPEN, "Open", "Open a circuit file"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_file_open_circuit));
    m_refActionGroup->add(Gtk::Action::create("ArchOpen", Gtk::Stock::OPEN, "Open", "Open an architecture file"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_file_open_arch));

    m_refActionGroup->add(Gtk::Action::create("DiagramSave", Gtk::Stock::SAVE, "_Save Picture",
                          "Save the circuit diagram to an image file"),
                          Gtk::AccelKey(0, (Gdk::ModifierType)0));
    m_refActionGroup->add(Gtk::Action::create("CircuitSave", Gtk::Stock::SAVE, "Save", "Save circuit"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_save_circuit));
    m_refActionGroup->add(Gtk::Action::create("ArchSave", Gtk::Stock::SAVE, "Save", "Save architecture"),
                          sigc::mem_fun(*this, &QCViewer::unimplemented));
    m_refActionGroup->add(Gtk::Action::create("DiagramSavePng", "P_NG",
                          "Save circuit diagram as a Portable Network Graphics file"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_save_png));
    m_refActionGroup->add(Gtk::Action::create("DiagramSaveSvg", "S_VG",
                          "Save circuit diagram as a Scalable Vector Graphics file"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_save_svg));
    m_refActionGroup->add(Gtk::Action::create("DiagramSavePs", "_Postscript", "Save circuit diagram as a Postscript file"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_save_ps));

    m_refActionGroup->add(Gtk::Action::create("SetArch", "Preset Arch"));
    m_refActionGroup->add(Gtk::Action::create("LNN", "LNN"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_set_arch_LNN));

    m_refActionGroup->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT, "Quit"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_file_quit));
    m_refActionGroup->add(Gtk::Action::create("ZoomIn", Gtk::Stock::ZOOM_IN, "Zoom In"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_zoom_in));
    m_refActionGroup->add(Gtk::Action::create("ZoomOut", Gtk::Stock::ZOOM_OUT, "Zoom Out"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_zoom_out));
    m_refActionGroup->add(Gtk::Action::create("Zoom100", Gtk::Stock::ZOOM_100, "100%"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_zoom_100));

    m_refActionGroup->add(Gtk::Action::create("SimulateMenu", "Simulate"));
    m_refActionGroup->add(Gtk::Action::create ("SimulateLoad", Gtk::Stock::ADD, "Load state", "Enter a state for input into the circuit"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_load_state));
    m_refActionGroup->add(Gtk::Action::create ("SimulateRun", Gtk::Stock::GOTO_LAST, "Run", "Simulate the entire circuit"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_run));
    m_refActionGroup->add(Gtk::Action::create ("SimulateStep", Gtk::Stock::GO_FORWARD, "Step", "Advance the simulation through a single gate"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_step));
    m_refActionGroup->add(Gtk::Action::create ("SimulateReset", Gtk::Stock::STOP, "Reset", "Reset the simulation to the start of the circuit"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_reset));
    m_refActionGroup->add(Gtk::Action::create ("SimulateDisplay",Gtk::Stock::ADD, "Display state", "Open a graphical display of the current state"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_simulate_show_stateView));

    m_refActionGroup->add(Gtk::Action::create("ArchitectureMenu", "Architecture"));
    m_refActionGroup->add(Gtk::ToggleAction::create ("DiagramParallel", "Show parallel guides"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_options_parallel));
    m_refActionGroup->add(Gtk::ToggleAction::create ("DiagramArch", Gtk::Stock::DIALOG_WARNING, "Show warnings", "Show architecture alignment warnings"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_options_arch));
    m_refActionGroup->add(Gtk::Action::create ("DiagramToggleLineLabels", "Toggle Line Labels"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_options_toggle_lab));

    m_refActionGroup->add(Gtk::Action::create("TextMode", "Text Mode"));
    m_refActionGroup->add(Gtk::Action::create("TextModeRegular", "Regular"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_textmode_regular));
    m_refActionGroup->add(Gtk::Action::create("TextModeLaTeX", "LaTeX"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_textmode_latex));


    m_refActionGroup->add(Gtk::Action::create("Help", "Help"));
    m_refActionGroup->add(Gtk::Action::create ("About", "About"),
                          sigc::mem_fun(*this, &QCViewer::on_menu_about));

    m_refUIManager = Gtk::UIManager::create();
    m_refUIManager->insert_action_group(m_refActionGroup);

    add_accel_group(m_refUIManager->get_accel_group());
}

void QCViewer::setup_menu_layout()
{
    Glib::ustring ui_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
        "    <menu action='File'>"
        "      <menuitem action='FileQuit'/>"
        "    </menu>"
        "    <menu action='Circuit'>"
        "      <menuitem action='CircuitNew'/>"
        "      <menuitem action='CircuitOpen'/>"
        "      <menuitem action='CircuitSave'/>"
        "    </menu>"
        "    <menu action='Diagram'>"
        "      <menu action='DiagramSave'>"
        "        <menuitem action='DiagramSavePng'/>"
        "        <menuitem action='DiagramSaveSvg'/>"
        "        <menuitem action='DiagramSavePs'/>"
        "        <separator/>"
        "      </menu>"
        "      <menu action='TextMode'>"
        "        <menuitem action='TextModeRegular'/>"
        "        <menuitem action='TextModeLaTeX'/>"
        "      </menu>"
        "      <menuitem action='DiagramParallel'/>"
        "      <menuitem action='DiagramArch'/>"
        "      <menuitem action='DiagramToggleLineLabels'/>"
        "    </menu>"
        "    <menu action='SimulateMenu'>"
        "      <menuitem action='SimulateDisplay'/>"
        "    </menu>"
        "    <menu action='Help'>"
        "    	<menuitem action='About'/>"
        "    </menu>"
        "  </menubar>"
        "  <toolbar  name='SimulateToolbar'>"
        "    <toolitem action='CircuitOpen'/>"
        "    <separator/>"
        "    <toolitem action='Zoom100'/>"
        "    <separator/>"
        "    <toolitem action='SimulateLoad'/>"
        "    <toolitem action='SimulateRun'/>"
        "    <toolitem action='SimulateStep'/>"
        "    <toolitem action='SimulateReset'/>"
        "    <toolitem action='SimulateDisplay'/>"
        "  </toolbar>"
        "</ui>";

    try {
        m_refUIManager->add_ui_from_string(ui_info);
    } catch(const Glib::Error& ex) {
        std::cerr << "building menus failed: " <<  ex.what();
    }
    Gtk::Widget* pMenubar = m_refUIManager->get_widget("/MenuBar");
    if(pMenubar)
        m_vbox.pack_start(*pMenubar, Gtk::PACK_SHRINK);
    m_SimulateToolbar = m_refUIManager->get_widget("/SimulateToolbar");

    m_GatesFrame.set_label ("Gates");
    m_GatesFrame.add (m_GatesTable);
    m_GatesTable.resize (2, 4);
    m_EditSidebar.pack_start (m_GatesFrame, Gtk::PACK_SHRINK);
    m_EditSidebar.set_homogeneous (false);

    //m_FlowFrame.set_label("Flow Control");
    //m_FlowFrame.add (m_FlowTable);
    //m_FlowTable.resize (1,1);
    //m_AddLoop.set_label ("Add Loop");
    //m_AddLoop.signal_clicked().connect(sigc::mem_fun(*this, &QCViewer::add_loop));
    //m_FlowTable.attach (m_AddLoop,0,1,0,1);
    //m_EditSidebar.pack_start (m_FlowFrame, Gtk::PACK_SHRINK);

    m_Subcirc.set_label("Subcircuit");
    m_Subcirc.add(m_SubcircTable);
    m_SubcircTable.resize (3,2);
    m_SubcircNameLbl.set_label("Name: ");
    m_SubcircExpandButton.set_label ("Expand");
    m_SubcircExpandButton.signal_clicked().connect(sigc::mem_fun(*this, &QCViewer::expand_subcirc));
    m_SubcircUnrollButton.set_label ("Unroll");
    m_SubcircUnrollButton.signal_clicked().connect(sigc::mem_fun(*this, &QCViewer::unroll_subcirc));
    m_SubcircExpandAllButton.set_label ("Expand All");
    m_SubcircExpandAllButton.signal_clicked().connect(sigc::mem_fun(*this, &QCViewer::expand_all_subcirc));
    m_SubcircNameEntry.signal_activate().connect(sigc::mem_fun(*this, &QCViewer::set_subcircuit_name));


    m_SubcircTable.attach(m_SubcircNameLbl,        0,1,0,1);
    m_SubcircTable.attach(m_SubcircNameEntry,      1,2,0,2);
    m_SubcircTable.attach(m_SubcircExpandButton,   0,2,2,3);
    m_SubcircTable.attach(m_SubcircExpandAllButton,0,2,3,4);
    m_SubcircTable.attach(m_SubcircUnrollButton,   0,2,4,5);

    m_EditSidebar.pack_start (m_Subcirc, Gtk::PACK_SHRINK);

    m_SimulationFrame.set_label ("Simulation");
    m_SimulationFrame.add (m_SimulationTable);
    m_SimulationTable.resize (1, 1);
    m_EditSidebar.pack_start (m_SimulationFrame, Gtk::PACK_SHRINK);
    btn_editbreakpoints.set_label ("Edit Breakpoints");
    btn_editbreakpoints.signal_clicked ().connect (sigc::mem_fun (*this, &QCViewer::update_mode));
    m_SimulationTable.attach (btn_editbreakpoints, 0,1,0,1);

    m_PropFrame.set_label ("Properties");
    m_PropFrame.add (m_PropTable);
    m_PropTable.resize (3,1);
    m_EditSidebar.pack_start (m_PropFrame, Gtk::PACK_SHRINK);
    btn_delete.set_label ("Delete");
    btn_editcontrols.set_label ("Edit Controls");
    m_IterLbl.set_label("Iterations: ");
    btn_delete.signal_clicked().connect(sigc::mem_fun(*this, &QCViewer::on_menu_delete));
    btn_editcontrols.signal_clicked().connect (sigc::mem_fun (*this, &QCViewer::update_mode));
    m_IterEntry.signal_activate().connect(sigc::mem_fun(*this, &QCViewer::set_loop_iter));
    m_PropTable.attach (btn_delete,0,2,0,1);
    m_PropTable.attach (btn_editcontrols,0,2,1,2);
    m_PropTable.attach (m_IterLbl,0,1,2,3);
    m_PropTable.attach (m_IterEntry,1,2,2,4);

    //Setup Rotation buttons
    m_RGateEditFrame.set_label ("Rotation");
    m_RGateEditFrame.add (m_RGateEditTable);
    m_RGateEditTable.resize (3,2);
    m_EditSidebar.pack_start (m_RGateEditFrame, Gtk::PACK_SHRINK);
    btn_RX.set_group (m_RAxisGroup);
    btn_RX.set_label ("X");
    btn_RY.set_group (m_RAxisGroup);
    btn_RY.set_label ("Y");
    btn_RZ.set_group (m_RAxisGroup);
    btn_RZ.set_label ("Z");
    btn_RX.signal_clicked ().connect (sigc::mem_fun(*this, &QCViewer::set_raxis));
    btn_RY.signal_clicked ().connect (sigc::mem_fun(*this, &QCViewer::set_raxis));
    btn_RZ.signal_clicked ().connect (sigc::mem_fun(*this, &QCViewer::set_raxis));
    m_RValLabel.set_text ("Value: ");
    m_RValEntry.signal_activate().connect (sigc::mem_fun(*this, &QCViewer::set_rval));
    m_RGateEditTable.attach (btn_RX, 0,1,0,1);
    m_RGateEditTable.attach (btn_RY, 1,2,0,1);
    m_RGateEditTable.attach (btn_RZ, 2,3,0,1);
    m_RGateEditTable.attach (m_RValLabel, 0,2,1,2);
    m_RGateEditTable.attach (m_RValEntry, 2,3,1,2);
    listTargets.push_back(Gtk::TargetEntry ("STRING"));
    listTargets.push_back(Gtk::TargetEntry ("text/plain"));
    c.drag_dest_set (listTargets);
}

void QCViewer::setup_gate_icons()
{
    gate_icons.push_back(new GateIcon(GateIcon::R));
    gate_icons.push_back(new GateIcon(GateIcon::NOT));
    gate_icons.push_back(new GateIcon(GateIcon::SWAP));
    gate_icons.push_back(new GateIcon(GateIcon::MEASURE));

    vector<string> names = UGateNames();
    vector<string> dnames = UGateDNames();
    for (unsigned int i = 0; i < names.size(); i++) {
        gate_icons.push_back(new GateIcon(names[i],dnames[i]));
    }
    for (unsigned int i = 0, y = 0, x = 0; i < gate_icons.size(); i++) {
        gate_buttons.push_back(manage(new Gtk::Button()));
        setup_gate_button (gate_buttons[i], gate_icons[i]);
        m_GatesTable.attach (*gate_buttons[i],x,x+1,y,y+1);
        x++;
        if (x > 3) {
            x = 0;
            y++;
        }
    }
    m_GatesTable.set_homogeneous ();
}

void QCViewer::register_stock_items()
{
    Glib::RefPtr<Gtk::IconFactory> factory = Gtk::IconFactory::create();
    add_stock_item(factory, "data/pan.png", "pan", "Pan");
    factory->add_default(); //Add factory to list of factories.
}

void QCViewer::add_stock_item(const Glib::RefPtr<Gtk::IconFactory>& factory, const std::string& filepath,
                              const Glib::ustring& id, const Glib::ustring& label)
{
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_file(filepath);
    if (pixbuf) {
        Gtk::IconSource source;
        source.set_pixbuf(pixbuf);
        source.set_size(Gtk::ICON_SIZE_SMALL_TOOLBAR);
        source.set_size_wildcarded(); //Icon may be scaled.
        Gtk::IconSet icon_set;
        icon_set.add_source(source); //More than one source per set is allowed.
        const Gtk::StockID stock_id(id);
        factory->add(stock_id, icon_set);
        Gtk::Stock::add(Gtk::StockItem(stock_id, label));
    }
}
