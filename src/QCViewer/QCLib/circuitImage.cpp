#include <iostream>
#include <assert.h>
#include <cairo-svg.h>
#include <cairo-ps.h>

#include "circuitImage.h"

#define L_COLOUR Colour(0,0,0,1) //XXX put somewhere better

using namespace std;


void CircuitImage::renderCairo(cairo_t* c)
{
    renderer = CAIRO;
    cr = c;
}

vector<gateRect> CircuitImage::draw (Circuit &c, bool drawArch, bool drawParallel, cairo_rectangle_t ext, double wirestart, double wireend, double scale, const vector<Selection> &selections, cairo_font_face_t * ft_default)
{
    assert(cr != NULL);
    //These are needed to get proper text extents
    cairo_scale (cr, scale, scale);
    cairo_set_font_face (cr,ft_default);
    cairo_set_font_size(cr, 18);
    // ---------------------------------
    drawPrims.clear();

    vector<gateRect> rects;
    rects = drawCirc (c, wirestart, wireend, true);
    drawbase (c, ext.width+ext.x, ext.height+ext.y+thickness, wirestart, wireend);
    if (drawParallel) {
        drawParallelSectionMarkings (rects, c.numLines(), c.getParallel());
    }
    if (drawArch) {
        drawArchitectureWarnings (rects, c.getArchWarnings());
    }
    if (!selections.empty()) {
        drawSelections (rects, selections);
    }
    cairoRender (cr);

    return rects;
}

void CircuitImage::drawbase (Circuit &c, double w, double h, double wirestart, double wireend)
{
    for (uint32_t i = 0; i < c.numLines(); i++) {
        double y = wireToY (i);
        drawPrims.push_front(makeLine(wirestart+xoffset, y, wireend, y, Colour(0,0,0,1)));
    }
    shared_ptr<DrawPrim> r = shared_ptr<DrawPrim>(new Rectangle(0,0,w,h,Colour(1,1,1,1) ,Colour(1,1,1,1)));
    drawPrims.push_front(r);
}

vector<gateRect> CircuitImage::drawCirc (Circuit &c, double &wirestart, double &wireend, bool forreal)
{
    vector <gateRect> rects;
    // input labels
    double xinit = 0.0;
    for (uint32_t i = 0; i < c.numLines(); i++) {
        string label = c.getLine(i).getInputLabel();
        TextExt extents = getExtents(label);
        double x = 0, y = 0;
        if (forreal) {
            x = wirestart - extents.w;
            y = wireToY(i) - (extents.h/2 + extents.y);
        }
        addText(label,x,y);
        xinit = max (xinit, extents.w);
    }
    if (!forreal) wirestart = xinit;
    // gates
    double xcurr = xinit+2.0*gatePad;
    uint32_t mingw, maxgw;
    unsigned int i = 0;
    double maxX = 0;
    if (c.numGates()>0) {
        for (uint32_t j = 0; j < c.columns.size(); j++) {
            maxX = 0.0;
            for (; i <= c.columns.at(j); i++) {
                shared_ptr<Gate> g = c.getGate (i);
                minmaxWire (g->controls, g->targets, mingw, maxgw);
                drawGate(g,xcurr,maxX,rects);
                if (c.simState.simulating && c.simState.gate == i+1) {
                    gateRect r = rects.back();
                    addRect(r.x0, r.y0, r.width, r.height, Colour (0.1,0.7,0.2,0.7), Colour (0.1, 0.7,0.2,0.3));
                }
            }
            xcurr += maxX - gatePad/2;
            if (c.getGate(c.columns.at(j))->breakpoint) {
                addLine(xcurr, wireToY(-1), xcurr, wireToY(c.numLines()), Colour(0.8,0,0,0.8));
            }
            xcurr += gatePad*1.5;
        }
    }


    xcurr -= maxX;
    xcurr += gatePad;
    gateRect fullCirc;
    if (rects.size() > 0) {
        fullCirc = rects[0];
        for (unsigned int i = 1; i < rects.size(); i++) {
            fullCirc = combine_gateRect(fullCirc,rects[i]);
        }
    }
    wireend = wirestart + fullCirc.width + gatePad*2;

    // output labels
    for (uint32_t i = 0; i < c.numLines (); i++) {
        string label = c.getLine(i).getOutputLabel();
        TextExt extents = getExtents(label);

        double x = wireend + xoffset;
        double y = wireToY(i) - (extents.h/2+extents.y);
        addText(label,x,y);
    }
    return rects;
}

void CircuitImage::drawArchitectureWarnings (const vector<gateRect> &rects, const vector<int> &badGates)
{
    for (uint32_t i = 0; i < badGates.size(); i++) {
        gateRect r = rects[badGates[i]];
        addRect(r.x0, r.y0, r.width, r.height, Colour (0.1,0.7,0.2,0.7), Colour (0.1, 0.7,0.2,0.3));
    }
}

void CircuitImage::drawParallelSectionMarkings (const vector<gateRect> &rects, int numLines, const vector<int> &pLines)
{
    for (uint32_t i = 0; i < pLines.size(); i++) {
        int gateNum = pLines[i];
        double x = (rects[gateNum].x0 + rects[gateNum].width + rects[gateNum+1].x0)/2;
        drawPWire (x, numLines);
    }
}

void CircuitImage::drawPWire (double x, int numLines)
{
    addLine(x, wireToY(0),x, wireToY(numLines-1), Colour(0.4, 0.4, 0.4,0.4));
}

void CircuitImage::drawSelections (const vector<gateRect> &rects, const vector<Selection> &selections)
{
    for (unsigned int i = 0; i < selections.size (); i++) {
        if (selections[i].gate < rects.size()) { //XXX Why is this needed?
            gateRect r = rects[selections[i].gate];
            addRect(r.x0, r.y0, r.width, r.height, Colour (0.1, 0.2, 0.7, 0.7), Colour (0.1,0.2,0.7,0.3));
            if (!selections[i].sub.empty() && !rects[selections[i].gate].subRects.empty()) {
                drawSelections (rects[selections[i].gate].subRects, selections[i].sub);
            }
        }
    }
}

cairo_rectangle_t CircuitImage::getCircuitSize (Circuit &c, double &wirestart, double &wireend, double scale, cairo_font_face_t * ft_default)
{
    drawPrims.clear();
    cairo_surface_t *unbounded_rec_surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR, NULL);
    cairo_t *context = cairo_create(unbounded_rec_surface);
    cairo_set_source_surface (context, unbounded_rec_surface, 0.0, 0.0);
    cairo_scale (context, scale, scale);
    cairo_set_font_face (context,ft_default);
    cairo_set_font_size(context, 18);
    drawCirc (c, wirestart, wireend, false); // XXX fix up these inefficienies!!
    cairoRender (context);
    cairo_rectangle_t ext;
    cairo_recording_surface_ink_extents (unbounded_rec_surface, &ext.x, &ext.y, &ext.width, &ext.height);
    cairo_destroy (context);
    cairo_surface_destroy (unbounded_rec_surface);
    return ext;
}
///////
void CircuitImage::savepng (Circuit &c, string filename, cairo_font_face_t * ft_default)
{
    double wirestart, wireend;
    cairo_rectangle_t ext = getCircuitSize (c, wirestart, wireend, 1.0,ft_default);
    cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, ext.width+ext.x, thickness+ext.height+ext.y);
    cairo_t* cr = cairo_create (surface);
    renderCairo(cr);
    cairo_set_source_surface (cr, surface, 0, 0);
    draw(c, false, false,  ext, wirestart, wireend, 1.0, vector<Selection>(), ft_default);
    write_to_png (surface, filename);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

void CircuitImage::write_to_png (cairo_surface_t* surf, string filename) const
{
    cairo_status_t status = cairo_surface_write_to_png (surf, filename.c_str());
    if (status != CAIRO_STATUS_SUCCESS) {
        cout << "Error saving to png." << endl;
        return;
    }
}

void CircuitImage::savesvg (Circuit &c, string filename, cairo_font_face_t * ft_default)
{
    double wirestart, wireend;
    cairo_rectangle_t ext = getCircuitSize (c,wirestart, wireend, 1.0, ft_default);
    cout << ext.width+ext.x << " : " << thickness+ext.height+ext.y << endl;
    cairo_surface_t* surface = cairo_svg_surface_create (filename.c_str(), ext.width+ext.x, thickness+ext.height+ext.y);
    cairo_t* context = cairo_create (surface);
    renderCairo(context);
    cairo_set_source_surface (context, surface, 0, 0);
    draw(c, false, false, ext, wirestart, wireend, 1.0, vector<Selection>(),ft_default);
    cairo_destroy (context);
    cairo_surface_destroy (surface);
}


void CircuitImage::saveps (Circuit &c, string filename,cairo_font_face_t * ft_default)
{
    double wirestart, wireend;
    cairo_rectangle_t ext = getCircuitSize (c,wirestart, wireend, 1.0,ft_default);
    cairo_surface_t* surface = make_ps_surface (filename, ext);
    cairo_t* cr = cairo_create(surface);
    renderCairo(cr);
    cairo_set_source_surface (cr, surface, 0,0);
    draw(c, false, false, ext, wirestart, wireend, 1.0, vector<Selection>(),ft_default);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

cairo_surface_t* CircuitImage::make_ps_surface (string file, cairo_rectangle_t ext) const
{
    cairo_surface_t *img_surface = cairo_ps_surface_create (file.c_str(), ext.width+ext.x, thickness+ext.height+ext.y);
    cairo_ps_surface_set_eps (img_surface, true);
    return img_surface;
}

void CircuitImage::drawGate(shared_ptr<Gate> g,double &xcurr,double &maxX, std::vector <gateRect> &rects)
{
    switch (g->type) {
    case Gate::UGATE:
    case Gate::RGATE:
        drawUGate (g,xcurr,maxX,rects);
        break;
    case Gate::SUBCIRC:
    default:
        drawSubcirc (static_pointer_cast<Subcircuit>(g),xcurr,maxX,rects);
        break;
    }
}

void CircuitImage::drawUGate(shared_ptr<Gate> g ,double &xcurr,double &maxX, vector <gateRect> &rects)
{
    gateRect r;
    switch (g->drawType) {
    case Gate::NOT:
        r = drawCNOT (g,xcurr);
        break;
    case Gate::FRED:
        r = drawFred (g,xcurr);
        break;
    case Gate::MEASURE:
        r = drawMeasure (g,xcurr);
        break;
    case Gate::DEFAULT:
    default:
        r = drawCU (g,xcurr);
        break;
    }
    rects.push_back(r);
    maxX = max (maxX, r.width);
}

gateRect CircuitImage::drawControls (shared_ptr<Gate> g,const gateRect &r)
{
    uint32_t center = r.x0+r.width/2.0;
    uint32_t top = r.y0;
    uint32_t bottem = r.y0+r.height;
    uint32_t mincw,maxcw,mintw,maxtw,minw,maxw;
    minmaxWire (g->controls, vector<unsigned int>() , mincw, maxcw);
    minmaxWire (vector<Control>(), g->targets, mintw, maxtw);
    minmaxWire (g->controls,g->targets, minw, maxw);
    if (!g->controls.empty()) {
        if (maxcw > maxtw) {
            addLine (center, wireToY (maxcw), center, bottem, L_COLOUR);
        }
        if (mincw < mintw) {
            addLine (center, wireToY (mincw), center, top, L_COLOUR);
        }
    }
    for (uint32_t i = 0; i < g->controls.size(); i++) {
        drawDot (center, wireToY(g->controls.at(i).wire), dotradius, g->controls.at(i).polarity);
    }
    gateRect rect;
    rect.x0 = center-dotradius;
    rect.y0 = wireToY(minw)-dotradius;
    rect.width = 2*dotradius;
    rect.height = wireToY(maxw) - wireToY(minw) + 2*(dotradius);
    return rect;
}

gateRect CircuitImage::drawControls (shared_ptr<Gate> g, uint32_t xc)
{
    uint32_t mincw,maxcw,mintw,maxtw,minw,maxw;
    minmaxWire (g->controls, vector<unsigned int>() , mincw, maxcw);
    minmaxWire (vector<Control>(), g->targets, mintw, maxtw);
    minmaxWire (g->controls,g->targets, minw, maxw);
    if (!g->controls.empty()) {
        if (maxcw > maxtw) {
            addLine (xc, wireToY (maxcw), xc, wireToY (maxtw)+radius, L_COLOUR);
        }
        if (mincw < mintw) {
            addLine (xc, wireToY (mincw), xc, wireToY (mintw)-radius, L_COLOUR);
        }
    }
    for (uint32_t i = 0; i < g->controls.size(); i++) {
        drawDot (xc, wireToY(g->controls.at(i).wire), dotradius, g->controls.at(i).polarity);
    }
    gateRect rect;
    rect.x0 = xc-dotradius;
    rect.y0 = wireToY(minw)-dotradius;
    rect.width = 2*dotradius;
    rect.height = wireToY(maxw) - wireToY(minw) + 2*(dotradius);
    return rect;
}

gateRect CircuitImage::drawMeasure (shared_ptr<Gate> g,uint32_t xc)
{
    double yc = wireToY(g->targets.at(0));
    addRect(xc-radius+thickness , yc-radius , 2*radius - 1.5*thickness, 2*radius,Colour(1,1,1,1) ,Colour(1,1,1,1));
    addLine (xc-radius, yc, xc+radius, yc+radius, L_COLOUR);
    addLine (xc-radius, yc, xc+radius, yc-radius, L_COLOUR);
    addLine (xc+radius, yc-radius, xc+radius, yc+radius, L_COLOUR);
    gateRect r;
    r.x0 = xc-radius-thickness;
    r.y0 = yc-radius-thickness;
    r.width = 2*(radius+thickness);
    r.height = r.width;
    return r;
}

gateRect CircuitImage::drawFred (shared_ptr<Gate> g, uint32_t xc)
{
    gateRect rect = drawControls (g, xc);
    uint32_t minw = g->targets.at(0);
    uint32_t maxw = g->targets.at(0);
    for (uint32_t i = 0; i < g->targets.size(); i++) {
        gateRect recttmp = drawX (xc, wireToY(g->targets.at(i)), radius);
        rect = combine_gateRect(rect, recttmp);
        minw = min (minw, g->targets.at(i));
        maxw = max (maxw, g->targets.at(i));
    }
    if (!g->controls.empty()) {
        addLine(xc, wireToY (minw)-radius, xc, wireToY (maxw), L_COLOUR);
    } else {
        addLine (xc, wireToY (minw), xc, wireToY (maxw), L_COLOUR);
    }
    return rect;
}

gateRect CircuitImage::drawCNOT (shared_ptr<Gate> g, uint32_t xc)
{
    gateRect rect = drawControls (g, xc);
    for (uint32_t i = 0; i < g->targets.size(); i++) {
        gateRect recttmp = drawNOT (xc, wireToY(g->targets.at(i)), radius);
        rect = combine_gateRect(rect, recttmp);
    }
    return rect;
}

gateRect CircuitImage::drawNOT (double xc, double yc, double radius)
{
    // Draw black border
    Colour black = Colour(0,0,0,1);
    Colour white = Colour(1,1,1,1);
    addCircle (radius, xc, yc, white, black);

    // Draw cross
    addLine(xc-radius, yc, xc+radius, yc, L_COLOUR);
    addLine(xc, yc-radius, xc, yc+radius, L_COLOUR);

    gateRect r;
    r.x0 = xc-radius-thickness;
    r.y0 = yc-radius-thickness;
    r.width = 2*(radius+thickness);
    r.height = r.width;
    return r;
}

gateRect CircuitImage::drawX (double xc, double yc, double radius)
{
    // Draw cross
    radius = radius*sqrt(2)/2;
    addLine(xc-radius, yc-radius, xc+radius, yc+radius, L_COLOUR);
    addLine(xc+radius, yc-radius, xc-radius, yc+radius, L_COLOUR);
    gateRect r;
    r.x0 = xc-radius-thickness;
    r.y0 = yc-radius-thickness;
    r.width = 2*(radius+thickness);
    r.height = r.width;
    return r;
}

gateRect CircuitImage::drawCU (shared_ptr<Gate> g, uint32_t xc)
{
    uint32_t minw, maxw;
    stringstream ss;
    ss << g->getDrawName();
    if (g->getLoopCount() > 1) {
        ss << " x" << g->getLoopCount();
    }
    vector<Control> dummy;
    minmaxWire (dummy, g->targets, minw, maxw); // only the targets
    // (XXX) need to do a  check in here re: target wires intermixed with not targets.

    double dw = wireToY(1)-wireToY(0);
    double yc = (wireToY (minw)+wireToY(maxw))/2;//-dw/2.0;
    double height = dw*(maxw-minw+Upad);

    // get width of this box
    TextExt extents = getExtents(ss.str());
    double width = extents.w+2*textPad;
    if (width < dw*Upad) {
        width = dw*Upad;
    }
    gateRect rect = drawControls (g, xc-radius+width/2.0);

    //Prepare gate rectangle draw prim
    Colour fill = Colour (1,1,1,1); //White fill
    Colour outline = Colour (0,0,0,1); //black outline
    addRect(xc-radius, yc-height/2, width, height,fill,outline);

    double x = (xc - radius + width/2) - extents.w/2 - extents.x;
    double y = yc - extents.h/2 - extents.y;
    addText(ss.str(),x,y);
    gateRect r;
    r.x0 = xc - thickness-radius;
    r.y0 = yc -height/2 - thickness;
    r.width = width + 2*thickness;
    r.height = height + 2*thickness;
    return combine_gateRect(rect, r);
}

void CircuitImage::drawSubcirc(shared_ptr<Subcircuit> s, double &xcurr,double &maxX, vector <gateRect> &rects)
{
    gateRect r;
    if (s->expand) {
        r = drawExp(s,xcurr);
    } else {
        r = drawCU(static_pointer_cast<Gate>(s),xcurr);
    }
    rects.push_back(r);
    maxX = max (maxX, r.width);
}

gateRect CircuitImage::drawExp(shared_ptr<Subcircuit> s,double xcurr)
{
    double maxX = 0.0;
    vector <gateRect> subRects;
    vector<unsigned int> para = s->getGreedyParallel();
    for(unsigned int j = 0; j <= (s->unroll * (s->getLoopCount()-1)); j++) {
        unsigned int currentCol = 0;
        for(unsigned int i = 0; i < s->numGates(); i++) {
            shared_ptr<Gate> g = s->getGate(i);
            drawGate(g,xcurr,maxX,subRects);
            if(para.size() > currentCol && i == para[currentCol]) {
                xcurr += maxX - gatePad/2;
                if (s->circ->getGate(i)->breakpoint) {
                    unsigned int maxTarget = 0;
                    for (unsigned int i = 0; i < s->targets.size(); i++) {
                        if (s->targets.at(i) > maxTarget) {
                            maxTarget = s->targets.at(i);
                        }
                    }
                    double bottem = wireToY(maxTarget+1);
                    double top = wireToY(maxTarget - s->circ->numLines());
                    addLine(xcurr, bottem, xcurr, top, Colour(0.8,0,0,0.8));
                }
                xcurr += gatePad*1.5;
                maxX = 0.0;
                currentCol++;
            }
            if (s->simState->simulating) {
                const gateRect r = subRects.back();
                if (!s->unroll && s->simState->gate == i + 1 ) {
                    addRect(r.x0, r.y0, r.width, r.height , Colour (0.1,0.7,0.2,0.7), Colour (0.1, 0.7,0.2,0.3));
                } else if (s->unroll && s->simState->gate + (s->simState->loop-1)*s->numGates()  == j*s->numGates() + i + 1)  {
                    addRect(r.x0, r.y0, r.width, r.height , Colour (0.1,0.7,0.2,0.7), Colour (0.1, 0.7,0.2,0.3));
                }
            }
        }
    }
    xcurr -= maxX;
    xcurr -= gatePad;
    gateRect r;
    if (subRects.size() > 0) {
        r = subRects.at(0);
        for (unsigned int i = 1; i < subRects.size(); i++) {
            //drawRect (cr, subRects->at(i), Colour(0.8,0,0,0.8), Colour (0.1, 0.7,0.2,0.3));
            r = combine_gateRect(r,(subRects)[i]);
        }
        drawSubCircBox(s,r);
        r.subRects = subRects;
    }
    return r;
}

void CircuitImage::drawSubCircBox(shared_ptr<Subcircuit> s, gateRect &r)
{
    addRect(r.x0, r.y0, r.width, r.height , Colour (1,1,1,0), Colour (0,0,0,1),true);
    gateRect rect = drawControls (static_pointer_cast<Gate>(s), r);
    stringstream ss;
    ss << s->getName();
    if (s->getLoopCount() > 1) {
        ss << " x" << s->getLoopCount();
        if (s->simState->simulating) {
            ss << " " << s->simState->loop << "/" << s->getLoopCount();
        }
    }
    TextExt extents = getExtents(ss.str());
    double x = r.x0;
    double y = r.y0 - (extents.h + extents.y) - 5.0;
    addText(ss.str(),x,y);
    r.height+=extents.h+10;
    r.y0-=extents.h+10;
    if (r.width < extents.w+4) {
        r.width = extents.w+4;
    }
    r = combine_gateRect(rect,r);
}

void CircuitImage::drawDot (double xc, double yc, double radius, bool negative)
{
    Colour black = Colour(0,0,0,1);
    Colour white = Colour(1,1,1,1);
    if (negative) {
        addCircle (radius, xc, yc, white, black);
    } else {
        addCircle (radius, xc, yc, black, black);
    }
}

shared_ptr<CircuitImage::DrawPrim> CircuitImage::makeLine (double x1,double y1,double x2, double y2, Colour c)
{
    return shared_ptr<DrawPrim>(new Line(x1, y1, x2, y2, c));
}

void CircuitImage::addLine (double x1,double y1,double x2, double y2, Colour c)
{
    drawPrims.push_back(makeLine(x1, y1, x2, y2, c));
}

void CircuitImage::addRect (double x,double y,double w, double h, Colour f, Colour o, bool dashed)
{
    shared_ptr<DrawPrim> r = shared_ptr<DrawPrim>(new Rectangle(x,y,w,h,f,o,dashed));
    drawPrims.push_back(r);
}

void CircuitImage::addText (string t, double x,double y)
{
    shared_ptr<DrawPrim> p = shared_ptr<DrawPrim>(new Text(t,x,y));
    drawPrims.push_back(p);
}

void CircuitImage::addCircle (double r, double x, double y, Colour f, Colour l)
{
    shared_ptr<DrawPrim> p = shared_ptr<DrawPrim>(new Circle(r,x,y,f,l));
    drawPrims.push_back(p);
}

CircuitImage::TextExt CircuitImage::getExtents(std::string str) const
{
    TextExt ext;
    switch (renderer) {
    case CAIRO:
    default:
        ext.h = 0;
        ext.w = 0;
        ext.y = 0;
        ext.x = 0;
        create_text_layout(cr, str, ext.w, ext.h);
        return ext;
    }
}

void CircuitImage::cairoRender (cairo_t *context) const
{
    list<std::shared_ptr<DrawPrim>>::const_iterator prim;
    for (prim=drawPrims.begin() ; prim != drawPrims.end(); prim++ ) {
        switch ((*prim)->type) {
        case DrawPrim::LINE:
            cairoLine(context,static_pointer_cast<Line>(*prim));
            break;
        case DrawPrim::RECTANGLE:
            cairoRectangle(context,static_pointer_cast<Rectangle>(*prim));
            break;
        case DrawPrim::TEXT:
            cairoText(context,static_pointer_cast<Text>(*prim));
            break;
        case DrawPrim::CIRCLE:
            cairoCircle(context,static_pointer_cast<Circle>(*prim));
            break;
        default:
            break;
        }
    }
}

void CircuitImage::cairoLine(cairo_t *context,std::shared_ptr<Line> line) const
{
    Colour c = line->colour;
    cairo_set_source_rgba (context,c.r,c.g,c.b,c.a);
    cairo_set_line_width (context, thickness);
    cairo_move_to (context, line->x1, line->y1);
    cairo_line_to (context, line->x2, line->y2);
    cairo_stroke (context);
}

void CircuitImage::cairoRectangle(cairo_t *context,std::shared_ptr<Rectangle> r) const
{
    const double dashes[] = { 4.0, 4.0 };
    if (r->dashed) { //turn dashes on
        cairo_set_dash (cr, dashes, 2, 0.0);
    }
    cairo_set_source_rgba (context, r->fill.r, r->fill.g, r->fill.b, r->fill.a);
    cairo_rectangle (context, r->x0, r->y0, r->width, r->height);
    cairo_fill (context);
    cairo_set_source_rgba (context, r->outline.r, r->outline.g, r->outline.b, r->outline.a);
    cairo_rectangle (context, r->x0, r->y0, r->width, r->height);
    cairo_stroke (context);
    if (r->dashed) { //turn dashes back off
        cairo_set_dash (cr, dashes, 0, 0.0);
    }
}

void CircuitImage::cairoText(cairo_t *context,std::shared_ptr<Text> t) const
{
    cairo_move_to (context, t->x, t->y);
    double w,h;
    PangoLayout *layout = create_text_layout(context, t->text, w, h);
    pango_cairo_show_layout (cr, layout);
    g_object_unref(layout);
}

void CircuitImage::cairoCircle(cairo_t *context,std::shared_ptr<Circle> c) const
{
    cairo_set_source_rgba (context, c->fill.r, c->fill.g, c->fill.b, c->fill.a);
    cairo_arc (context, c->x, c->y, c->r, 0, 2*M_PI);
    cairo_fill (context);
    cairo_set_source_rgba (context, c->outline.r, c->outline.g, c->outline.b, c->outline.a);
    cairo_set_line_width(context, thickness);
    cairo_arc (context, c->x, c->y, c->r, 0, 2*M_PI);
    cairo_stroke (context);
}
