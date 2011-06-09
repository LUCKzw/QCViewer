#ifndef WINDOW__INCLUDED
#define WINDOW__INCLUDED

#include <gtkmm.h>
#include "circuitwidget.h"
#include "stateView.h"
#include "GateIcon.h"

class QCViewer : public Gtk::Window
{
public:
  QCViewer ();
  virtual ~QCViewer();

protected:

  // Signal handlers:
  void on_menu_file_open_circuit ();
  void on_menu_file_open_arch ();
	void on_menu_mode_edit ();
	void on_menu_mode_simulate ();
//  void on_menu_file_save ();
  void on_menu_file_quit ();
  void on_menu_save_png ();
  void on_menu_save_svg ();
  void on_menu_options_parallel ();
  void on_menu_options_arch ();
  void on_menu_zoom_in ();
  void on_menu_zoom_out ();
  void on_menu_zoom_100 ();
  void on_menu_run ();
  void on_menu_step();
  void on_menu_reset();
	void on_menu_delete();
  void unimplemented ();
  void on_menu_simulate_show_stateView ();
	void on_menu_pan ();
	void on_menu_inserttest ();
	void on_menu_load_state ();
//  void on_architecture_load ();

//  void on_menu_simulation_reset ();
//  void on_menu_simulation_run ();
//  void on_menu_simulation_step ();

//  void on_options_arch_warnings ();
//  void on_options_parallel_guides ();

  // Child widgets:
  Gtk::VBox m_vbox;
	Gtk::HBox m_hbox;
  Gtk::TextView m_cmdOut;
  Gtk::Entry m_cmdIn;
  Glib::RefPtr<Gtk::UIManager> m_refUIManager;
  Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;

  CircuitWidget c;
	stateView sView;
	Gtk::Statusbar m_statusbar;

	Gtk::Widget* m_EditToolbar;
	Gtk::Widget* m_SimulateToolbar;
	Gtk::ToolPalette m_Palette;

	GateIcon NOTicon, Hicon, Xicon, Yicon, Zicon, Ricon, SWAPicon;
private:
  enum Mode { EDIT_MODE, SIMULATE_MODE } mode;
	State *state;
  bool drawparallel;
  bool drawarch;
};

#endif
