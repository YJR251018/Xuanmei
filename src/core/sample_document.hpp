// The built-in sample document used for acceptance and demonstration.
//
// The geometry below is normative: docs/MVP_DESIGN.md section 8 fixes every
// parameter, and the acceptance figure A3 (total length 1900 + 150*pi mm) is
// defined by exactly these seven entities. Changing anything here changes the
// acceptance baseline, so the doc and this function must be edited together.

#pragma once

#include "core/document.hpp"

namespace xm::core {

// Builds the sample document deterministically (no randomness, no file I/O).
//
// Layers: OUTLINE, HOLES, SLOT.
// Entities, in id order:
//   1-4  LINE        OUTLINE  the 300 mm square, in order
//   5    CIRCLE      HOLES    centre (450,150), r = 50
//   6    ARC         HOLES    centre (450,350), r = 100, 0 -> 90 degrees
//   7    LWPOLYLINE  SLOT     closed slot (0,-200) (200,-200) (200,-50) (0,-50)
GeometryDocument makeSampleDocument();

}  // namespace xm::core
