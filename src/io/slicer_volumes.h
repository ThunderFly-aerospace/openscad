#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "geometry/linalg.h"

class AbstractNode;
class Geometry;
class PolySet;
class Tree;

/*!
   Slicer volumes (PrusaSlicer modifiers, negative volumes, support
   blockers/enforcers) from slicer_modifier() nodes, ready for 3MF export.

   PrusaSlicer reads them from Metadata/Slic3r_PE_model.config, which describes
   the parts of an object as inclusive triangle ranges of ONE mesh, each part
   using its own contiguous block of vertices (3mf.cpp, _generate_volumes).
 */
struct SlicerVolume {
  std::string volumeType;  // PrusaSlicer volume_type, e.g. "ParameterModifier"
  std::string name;
  std::vector<std::pair<std::string, std::string>> settings;
  std::shared_ptr<const PolySet> geometry;  // triangulated, world coordinates
  Color4f displayColor;                      // slicer_modifier(color=...) for drawings
};
using SlicerVolumes = std::vector<SlicerVolume>;

// Evaluate the region of every slicer_modifier() under `root` and transform it
// into world coordinates. Empty regions are skipped with a warning.
std::shared_ptr<const SlicerVolumes> collectSlicerVolumes(const Tree& tree,
                                                         const std::shared_ptr<const AbstractNode>& root);

// The model mesh with the part inside each slicer volume recolored with its
// display color (later volumes win), as one mesh: (model - M) + (model * M).
// Used by the drawings, which project a single colored mesh. Needs Manifold;
// without it, or when the model is not a valid manifold, returns `model`.
std::shared_ptr<const PolySet> colorSlicerRegions(const std::shared_ptr<const PolySet>& model,
                                                  const SlicerVolumes& volumes);

// One part of a PrusaSlicer object: a triangle range of the combined mesh.
struct SlicerPart {
  size_t firstTriangle;
  size_t lastTriangle;  // inclusive
  std::string volumeType;
  std::string name;
  std::vector<std::pair<std::string, std::string>> settings;
};

// A single PrusaSlicer object: the model parts followed by the slicer volumes,
// concatenated into one triangulated PolySet (vertex blocks never shared).
struct SlicerObject {
  std::shared_ptr<PolySet> mesh;
  std::vector<SlicerPart> parts;
};

// Build the combined object from the exported model geometry (a GeometryList
// contributes one ModelPart per top-level item) and the slicer volumes.
// With `sortParts`, each part is sorted on its own (predictable output) before
// concatenation, so the ranges stay valid.
SlicerObject buildSlicerObject(const std::shared_ptr<const Geometry>& model,
                               const SlicerVolumes& volumes, bool sortParts);

// Contents of Metadata/Slic3r_PE_model.config for one object.
std::string prusaSlicerModelConfig(int objectId, const std::string& objectName,
                                   const std::vector<SlicerPart>& parts);

// Path and relationship type under which the config is attached to the 3MF.
extern const char *const PRUSASLICER_MODEL_CONFIG_PATH;
extern const char *const PRUSASLICER_MODEL_CONFIG_RELATIONSHIP;
