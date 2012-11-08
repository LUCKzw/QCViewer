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
/*! \file circuitDrawer.h
    \brief Defines the circuit drawing class for rendering images of circuits
*/

#ifndef CIRCUITDRAWER_H
#define CIRCUITDRAWER_H

#include <cairo.h>
#include <vector>
#include <list>
#include <string>
#include <memory>
#include "common.h"
#include "circuit.h"
#include "subcircuit.h"
#include "draw_constants.h"
#include "draw_common.h"

class CircuitImage
{
public:
    enum Renderer {CAIRO};
    Renderer renderer;
    std::vector<gateRect> drawCirc (cairo_t*, Circuit&, double& , double& , bool );
    std::vector<gateRect> draw (Circuit&, bool, bool, cairo_rectangle_t, double, double, double, const std::vector<Selection>&, cairo_font_face_t *);
    cairo_rectangle_t getCircuitSize (Circuit& c, double&, double&, double, cairo_font_face_t*);
    void savepng (Circuit&, std::string, cairo_font_face_t*);
    void savesvg (Circuit&, std::string, cairo_font_face_t*);
    void saveps  (Circuit&, std::string, cairo_font_face_t*);

    void renderCairo(cairo_t*);

private:
    class DrawPrim
    {
    public:
        enum PrimType {LINE, RECTANGLE, TEXT, CIRCLE};
        PrimType type;
    };

    class Line : public DrawPrim
    {
    public:
        Line(double nx1,double ny1,double nx2, double ny2, Colour c)
            : x1(nx1), x2(nx2), y1(ny1), y2(ny2), colour(c) {
            type = LINE;
        }
        double x1,x2,y1,y2;
        Colour colour;
    };

    class Rectangle : public DrawPrim
    {
    public:
        Rectangle(double nx0,double ny0,double nwidth, double nheight, Colour f, Colour o)
            : x0(nx0), y0(ny0), height(nheight), width(nwidth), fill(f), outline(o) {
            type = RECTANGLE;
        }
        double x0,y0,height,width;
        Colour fill;
        Colour outline;
    };

    class Text : public DrawPrim
    {
    public:
        Text(std::string s, double x0, double y0)
            : text(s), x(x0), y(y0) {
            type = TEXT;
        }
        std::string text;
        double x, y;
    };

    class Circle : public DrawPrim
    {
    public:
        Circle(double r0, double x0, double y0, Colour f, Colour l)
            : r(r0), x(x0), y(y0), fill(f), outline(l) {
            type = CIRCLE;
        }
        double r;
        double x, y;
        Colour fill;
        Colour outline;
    };

    class TextExt
    {
    public:
        double x,y,h,w;
    };

    cairo_t* cr;

    std::list<std::shared_ptr<DrawPrim>> drawPrims;

    std::shared_ptr<DrawPrim> makeLine (double x1,double y1,double x2, double y2, Colour c);
    void addLine (double x1,double y1,double x2, double y2, Colour c);
    void addRect (double x1,double y1,double x2, double y2, Colour f, Colour o);
    void addText (std::string t, double x,double y);
    void addCircle (double r0, double x0, double y0, Colour f, Colour l);
    //RENDERS
    void cairoRender(cairo_t*) const;
    void cairoLine(cairo_t*,std::shared_ptr<Line>) const;
    void cairoRectangle(cairo_t*,std::shared_ptr<Rectangle>) const;
    void cairoText(cairo_t*,std::shared_ptr<Text>) const;
    void cairoCircle(cairo_t*,std::shared_ptr<Circle>) const;

    TextExt getExtents(std::string) const;

    void drawbase (cairo_t*, Circuit&, double, double, double, double);
    void drawArchitectureWarnings (const std::vector<gateRect>&, const std::vector<int>&);
    void drawParallelSectionMarkings (const std::vector<gateRect>&, int, const std::vector<int>&);
    void drawPWire (double, int);
    void drawSelections (cairo_t* , const std::vector<gateRect>& , const std::vector<Selection>&);


    void drawGate(std::shared_ptr<Gate> g,cairo_t *cr,double &xcurr,double &maxX, std::vector <gateRect> &rects);
    void drawUGate(std::shared_ptr<Gate> g,double &xcurr,double &maxX, std::vector <gateRect> &rects);
    gateRect drawControls (std::shared_ptr<Gate> g, const gateRect &r);
    gateRect drawControls (std::shared_ptr<Gate> g, uint32_t xc);
    gateRect drawFred (std::shared_ptr<Gate> g, uint32_t xc);
    gateRect drawCNOT (std::shared_ptr<Gate> g, uint32_t xc);
    gateRect drawNOT (double xc, double yc, double radius);
    gateRect drawX (double xc, double yc, double radius);
    gateRect drawCU (std::shared_ptr<Gate> g,uint32_t xc);
    void drawSubcirc(std::shared_ptr<Subcircuit> s, cairo_t *cr,double &xcurr,double &maxX, std::vector <gateRect> &rects);
    gateRect drawExp(std::shared_ptr<Subcircuit> s,cairo_t *cr,double xcurr);
    void drawSubCircBox(std::shared_ptr<Subcircuit> s, cairo_t* cr, gateRect &r);
    void drawDot (double xc, double yc, double radius, bool negative);

    void write_to_png (cairo_surface_t*, std::string) const;
    cairo_surface_t* make_ps_surface (std::string, cairo_rectangle_t) const;
};


#endif
