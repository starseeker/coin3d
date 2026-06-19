#ifndef OBOL_CADASSEMBLY_H
#define OBOL_CADASSEMBLY_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Obol/base/Export.h>
#include <Obol/cad/CadTypes.h>

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace obol {

enum class CadDrawMode {
    Shaded,
    Wireframe,
    ShadedWithEdges
};

enum class CadPickMode {
    Auto,
    Edge,
    Triangle,
    Bounds,
    Hybrid
};

class OBOL_V2_API CadAssembly {
public:
    void setDrawMode(CadDrawMode mode);
    CadDrawMode drawMode() const;

    void setPickMode(CadPickMode mode);
    CadPickMode pickMode() const;

    void setEdgePickTolerance(float pixels);
    float edgePickTolerance() const;

    void setWireframeOcclusion(bool enabled);
    bool wireframeOcclusion() const;

    void setLodEnabled(bool enabled);
    bool lodEnabled() const;

    void upsertPart(PartId id, const CadPartGeometry & geometry);
    void removePart(PartId id);

    InstanceId upsertInstanceAuto(const CadInstanceRecord & record);
    void upsertInstance(InstanceId id, const CadInstanceRecord & record);
    void removeInstance(InstanceId id);
    void updateInstanceTransform(InstanceId id, const CadMatrix4 & localToRoot);
    void updateInstanceStyle(InstanceId id, const CadInstanceStyle & style);

    void setSelectedInstances(const std::vector<InstanceId> & ids);
    void setHiddenInstances(const std::vector<InstanceId> & ids);

    size_t partCount() const;
    size_t instanceCount() const;
    std::vector<PartId> partIds() const;
    std::vector<InstanceId> instanceIds() const;
    const CadPartGeometry * partGeometry(PartId id) const;
    std::optional<CadInstanceRecord> getInstanceRecord(InstanceId id) const;
    const std::vector<InstanceId> & selectedInstances() const;
    const std::vector<InstanceId> & hiddenInstances() const;

    // Legacy bridge for the initial v2 rollout.  The returned node is owned by
    // the caller or by the scene graph it is inserted into.
    NativeNodeHandle createLegacyNode() const;

private:
    CadDrawMode drawMode_ = CadDrawMode::Wireframe;
    CadPickMode pickMode_ = CadPickMode::Auto;
    float edgePickTolerancePx_ = 5.0f;
    bool wireframeOcclusion_ = true;
    bool lodEnabled_ = false;
    std::unordered_map<PartId, CadPartGeometry> parts_;
    std::unordered_map<InstanceId, CadInstanceRecord> instances_;
    std::vector<InstanceId> selectedInstances_;
    std::vector<InstanceId> hiddenInstances_;
};

} // namespace obol

#endif // OBOL_CADASSEMBLY_H
