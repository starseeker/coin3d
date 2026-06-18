#ifndef OBOL_CADASSEMBLY_H
#define OBOL_CADASSEMBLY_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Inventor/SbBasic.h>

#include <Obol/cad/SoCADAssembly.h>

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

class OBOL_DLL_API CadAssembly {
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

    void upsertPart(PartId id, const PartGeometry & geometry);
    void removePart(PartId id);

    InstanceId upsertInstanceAuto(const InstanceRecord & record);
    void upsertInstance(InstanceId id, const InstanceRecord & record);
    void removeInstance(InstanceId id);
    void updateInstanceTransform(InstanceId id, const SbMatrix & localToRoot);
    void updateInstanceStyle(InstanceId id, const InstanceStyle & style);

    void setSelectedInstances(const std::vector<InstanceId> & ids);
    void setHiddenInstances(const std::vector<InstanceId> & ids);

    size_t partCount() const;
    size_t instanceCount() const;
    const PartGeometry * partGeometry(PartId id) const;
    std::optional<InstanceRecord> getInstanceRecord(InstanceId id) const;

    // Legacy bridge for the initial v2 rollout.  The returned node is owned by
    // the caller or by the scene graph it is inserted into.
    SoCADAssembly * createLegacyNode() const;

private:
    CadDrawMode drawMode_ = CadDrawMode::Wireframe;
    CadPickMode pickMode_ = CadPickMode::Auto;
    float edgePickTolerancePx_ = 5.0f;
    bool wireframeOcclusion_ = true;
    bool lodEnabled_ = false;
    std::unordered_map<PartId, PartGeometry> parts_;
    std::unordered_map<InstanceId, InstanceRecord> instances_;
    std::vector<InstanceId> selectedInstances_;
    std::vector<InstanceId> hiddenInstances_;
};

} // namespace obol

#endif // OBOL_CADASSEMBLY_H
