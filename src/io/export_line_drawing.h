#pragma once

#include <string>

class PolySet;

/*!
   Write a compact vector line drawing of `ps` to `path`: the three orthographic
   views (front / side / top) laid out like a technical sheet, drawn from the
   mesh's silhouette + crease + boundary edges with hidden lines removed (see
   FeatureEdges and the CPU depth buffer in export_line_drawing.cc). Units are
   millimetres at 1:1. `creaseAngleDeg` is the dihedral threshold above which an
   edge counts as a feature edge.

   The output format is chosen from the file extension: ".dxf" writes DXF LINE
   entities, anything else writes SVG. Returns false if the file can't be written.
 */
bool exportLineDrawing(const PolySet& ps, double creaseAngleDeg, const std::string& path);
