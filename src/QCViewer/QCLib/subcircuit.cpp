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

Authors: Alex Parent
---------------------------------------------------------------------*/


#include "subcircuit.h"
#include "simulate.h"
#include <iostream> //XXX
#include "draw_constants.h"

using namespace std;

Subcircuit::Subcircuit(Circuit* n_circ, const map <unsigned int,unsigned int>& n_linemap, unsigned int loops)
{
    drawType = D_SUBCIRC;
    type = SUBCIRC;
    circ = n_circ;
    lineMap = n_linemap;
    loop_count = loops;
    expand = false;
}

Gate* Subcircuit::clone() const
{
    Subcircuit* g = new Subcircuit(circ,lineMap,loop_count);
    g->controls = controls;
    g->targets = targets;
    g->expand = expand;
    return g;
}

string Subcircuit::getName() const
{
    if (circ != NULL) return circ->getName();
    return "NULL";
}

void Subcircuit::setName(string n_name)
{
    circ->setName(n_name);
}

State Subcircuit::applyToBasis(index_t in) const
{
    State s = State(1,in);
    s = applySubcirc (s);
    return s;
}

State Subcircuit::applySubcirc(const State& in) const
{
    State s = in;
    for (unsigned int i = 0; i < circ->numGates(); i++) {
        s = ApplyGate(s,getGate(i));
    }
    return s;
}

Gate* Subcircuit::getGate(int pos) const
{
    Gate *g = circ->getGate(pos)->clone();
    for (unsigned int i = 0; i < g->targets.size(); i++) {
        map<unsigned int,unsigned int>::const_iterator it = lineMap.find(g->targets[i]);
        if (it!= lineMap.end()) {
            g->targets[i] = it->second;
        }
    }
    for (unsigned int i = 0; i < g->controls.size(); i++) {
        map<unsigned int,unsigned int>::const_iterator it = lineMap.find(g->controls[i].wire);
        if (it!= lineMap.end()) {
            g->controls[i].wire = it->second;
        }
    }
    if (g->type == SUBCIRC) { //Combine the maps if it is a subcircuit so we have the correct global map
        Subcircuit* s = (Subcircuit*)g;
        map<unsigned int,unsigned int>::iterator it = s->lineMap.begin();
        for (; it!=s->lineMap.end(); it++) {
            map<unsigned int,unsigned int>::const_iterator currIt = lineMap.find(it->second);
            if (currIt != lineMap.end()) {
                it->second = currIt->second;
            }
        }
    }
    return g;
}

int Subcircuit::numGates() const
{
    return circ->numGates();
}

vector<int> Subcircuit::getGreedyParallel() const  //Returns a vector of ints specifying the last gate in each parallel block.
{
    return circ->getGreedyParallel();
}


Circuit* Subcircuit::getCircuit()
{
    return circ;
}


void Subcircuit::draw(cairo_t *cr,double &xcurr,double &maxX, vector <gateRect> &rects) const
{
    gateRect r;
    if (expand) {
        r = drawExp(cr,xcurr);
    } else {
        r = drawBoxed(cr,xcurr);
    }
    rects.push_back(r);
    maxX = max (maxX, r.width);
}

gateRect Subcircuit::drawExp(cairo_t *cr,double xcurr) const
{
    gateRect r;
    double maxX = 0.0;
    vector <gateRect>*subRects = new vector<gateRect>;
    vector<int> para = getGreedyParallel();
    unsigned int currentCol = 0;
    for(int i = 0; i < numGates(); i++) {
        getGate(i)->draw(cr,xcurr,maxX,*subRects);
        if(para.size() > currentCol && i == para[currentCol]) {
            xcurr += maxX;
            maxX = 0.0;
            xcurr += gatePad;
            currentCol++;
        }
    }
    xcurr -= maxX;
    xcurr -= gatePad;
    r = (*subRects)[0];
    for (unsigned int i = 1; i < subRects->size(); i++) {
        r = combine_gateRect(r,(*subRects)[i]);
    }
    drawSubCircBox(cr, r);
    r.subRects = subRects;
    return r;
}

void Subcircuit::drawSubCircBox(cairo_t* cr, gateRect &r) const
{
    double dashes[] = { 4.0, 4.0 };
    cairo_set_dash (cr, dashes, 2, 0.0);
    cairo_set_line_width (cr, 2);
    cairo_rectangle (cr, r.x0, r.y0, r.width, r.height);
    cairo_stroke (cr);
    cairo_set_dash (cr, dashes, 0, 0.0);
    stringstream ss;
    ss << getName();
    if (getLoopCount() > 1) {
        ss << " x" << getLoopCount();
    }
    //cairo_set_font_size(cr, 22);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, ss.str().c_str(), &extents);
    double x = r.x0;
    double y = r.y0 - (extents.height + extents.y_bearing) - 5.0;
    cairo_move_to(cr, x, y);
    cairo_show_text (cr, ss.str().c_str());
    r.height+=extents.height+10;
    r.y0-=extents.height+10;
    if (r.width < extents.width+4) {
        r.width = extents.width+4;
    }
}

gateRect Subcircuit::drawBoxed (cairo_t *cr, uint32_t xc) const
{
    uint32_t minw, maxw;
    string name = getName();
    vector<Control> dummy;
    minmaxWire (dummy, targets, minw, maxw); // only the targets
    // (XXX) need to do a  check in here re: target wires intermixed with not targets.

    double dw = wireToY(1)-wireToY(0);
    double yc = (wireToY (minw)+wireToY(maxw))/2;//-dw/2.0;
    double height = dw*(maxw-minw+Upad);

    // get width of this box
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, name.c_str(), &extents);
    double width = extents.width+2*textPad;
    if (width < dw*Upad) {
        width = dw*Upad;
    }
    gateRect rect = drawControls (cr, xc-radius+width/2.0);
    cairo_rectangle (cr, xc-radius, yc-height/2, width, height);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_fill(cr);
    cairo_rectangle (cr, xc-radius, yc-height/2, width, height);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width (cr, thickness);
    cairo_stroke(cr);

    double x = (xc - radius + width/2) - extents.width/2 - extents.x_bearing;
    double y = yc - extents.height/2 - extents.y_bearing;
    cairo_move_to(cr, x, y);
    cairo_show_text (cr, name.c_str());
    gateRect r;
    r.x0 = xc - thickness-radius;
    r.y0 = yc -height/2 - thickness;
    r.width = width + 2*thickness;
    r.height = height + 2*thickness;
    return combine_gateRect(rect, r);
}
