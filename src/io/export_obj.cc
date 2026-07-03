/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *  Copyright (C) 2021      Konstantin Podsvirov <konstantin@podsvirov.pro>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "Feature.h"
#include "geometry/Geometry.h"
#include "geometry/PolySet.h"
#include "geometry/PolySetUtils.h"
#include "io/export.h"
#include "utils/printutils.h"

namespace {

// Material name for a given per-face color index. -1 (no specific color) maps
// to a neutral "default" material so those faces still get a clean Blender
// material instead of the OBJ implicit default.
std::string obj_material_name(int color_index)
{
  return color_index < 0 ? "default" : "color_" + std::to_string(color_index);
}

}  // namespace

void export_obj(const std::shared_ptr<const Geometry>& geom, std::ostream& output,
                const std::string& mtl_path)
{
  // FIXME: In lazy union mode, should we export multiple objects?

  std::shared_ptr<const PolySet> out = PolySetUtils::getGeometryAsPolySet(geom);
  if (!out->isTriangular()) {
    // While the OBJ format allows for faces to have more than 3
    // vertices, this seems to confuse a number of applications
    // we care about, so for now this will just always tesselate
    // faces to be composed of triangles only.
    //
    // See: https://github.com/openscad/openscad/issues/5993
    out = PolySetUtils::tessellate_faces(*out);
  }
  if (Feature::ExperimentalPredictibleOutput.is_enabled()) {
    out = createSortedPolySet(*out);
  }

  // Only emit materials when we have somewhere to write the .mtl (i.e. a real
  // output file, not stdout) AND the geometry actually carries per-face colors.
  // Otherwise the output is byte-for-byte identical to the legacy exporter.
  const bool has_color =
    !mtl_path.empty() && !out->color_indices.empty() && !out->colors.empty();

  output << "# OpenSCAD obj exporter\n";
  if (has_color) {
    output << "mtllib " << std::filesystem::path(mtl_path).filename().string() << "\n";
  }

  for (const auto& v : out->vertices) {
    output << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";
  }

  if (!has_color) {
    for (const auto& poly : out->indices) {
      output << "f ";
      for (const auto idx : poly) {
        output << " " << idx + 1;
      }
      output << "\n";
    }
    return;
  }

  // Group faces by color index. std::map keeps the keys ordered (-1 first, then
  // ascending palette indices), giving deterministic output.
  std::map<int, std::vector<std::size_t>> faces_by_color;
  for (std::size_t i = 0; i < out->indices.size(); ++i) {
    const int ci = i < out->color_indices.size() ? out->color_indices[i] : -1;
    faces_by_color[ci].push_back(i);
  }

  for (const auto& [ci, faces] : faces_by_color) {
    output << "usemtl " << obj_material_name(ci) << "\n";
    for (const auto fi : faces) {
      output << "f ";
      for (const auto idx : out->indices[fi]) {
        output << " " << idx + 1;
      }
      output << "\n";
    }
  }

  // Write the companion .mtl file, emitting only the materials actually used.
  std::ofstream mtl(std::filesystem::path(mtl_path), std::ios::out | std::ios::trunc);
  if (!mtl.is_open()) {
    LOG(message_group::Warning, "Can't open file \"%1$s\" for export", mtl_path);
    return;
  }
  mtl << "# OpenSCAD obj exporter\n";
  for (const auto& [ci, faces] : faces_by_color) {
    mtl << "newmtl " << obj_material_name(ci) << "\n";
    float r = 0.8f, g = 0.8f, b = 0.8f, a = 1.0f;
    if (ci >= 0 && static_cast<std::size_t>(ci) < out->colors.size()) {
      const auto& col = out->colors[ci];
      if (col.isValid()) {
        r = col.r();
        g = col.g();
        b = col.b();
        a = col.a();
      }
    }
    mtl << "Kd " << r << " " << g << " " << b << "\n";
    if (a < 1.0f) {
      mtl << "d " << a << "\n";
    }
    mtl << "\n";
  }
}
