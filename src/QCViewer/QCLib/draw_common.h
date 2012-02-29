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


#ifndef DRAW_COMMON_H
#define DRAW_COMMON_H

#include <cairo.h>
#include <vector>
#include "types.h"
#include "gate.h"
#include "../common.h"

gateRect combine_gateRect (const gateRect &a, const gateRect &b);
void drawDot (cairo_t *cr, double xc, double yc, double radius, bool negative);
double wireToY (uint32_t x);
void drawWire (cairo_t *cr, double x1, double y1, double x2, double y2);
#endif


