#ifndef OBOL_CADTYPES_H
#define OBOL_CADTYPES_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Obol/cad/CadIds.h>
#include <Obol/scene/Scene.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace obol {

struct CadBounds3 {
    Vec3 minimum;
    Vec3 maximum;
    bool empty = true;

    void setBounds(const Vec3 & minValue, const Vec3 & maxValue)
    {
        minimum = minValue;
        maximum = maxValue;
        empty = false;
    }
};

struct CadMatrix4 {
    std::array<float, 16> values;

    CadMatrix4()
    {
        makeIdentity();
    }

    void makeIdentity()
    {
        values = {1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f};
    }
};

struct CadWirePolyline {
    std::vector<Vec3> points;
    uint32_t edgeId = 0;
};

struct CadWireRep {
    std::vector<CadWirePolyline> polylines;
    CadBounds3 bounds;
};

struct CadTriMesh {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<uint32_t> indices;
    CadBounds3 bounds;
};

struct CadPartGeometry {
    std::optional<CadWireRep> wire;
    std::optional<CadTriMesh> shaded;
};

struct CadInstanceStyle {
    bool hasColorOverride = false;
    Color color = {0.8f, 0.8f, 0.8f, 1.0f};
    float lineWidth = 1.0f;
};

struct CadInstanceRecord {
    PartId part;
    CadMatrix4 localToRoot;

    InstanceId parent;
    std::string childName;
    uint32_t occurrenceIndex = 0;
    uint8_t boolOp = 0;

    CadInstanceStyle style;
};

} // namespace obol

#endif // OBOL_CADTYPES_H
