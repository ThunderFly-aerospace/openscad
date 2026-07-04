#pragma once

#include <string>

class PolySet;

/*!
   Write a compact vector line drawing of `ps` to an SVG file at `path`: the
   three orthographic views (front / side / top) laid out like a technical sheet,
   each drawn from the mesh's silhouette + crease + boundary edges (see
   FeatureEdges). Units are millimetres at 1:1. `creaseAngleDeg` is the dihedral
   threshold above which an edge counts as a feature edge.

   Returns false if the file can't be written.
 */
bool exportLineDrawingSvg(const PolySet& ps, double creaseAngleDeg, const std::string& path);
