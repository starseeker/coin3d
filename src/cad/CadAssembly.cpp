#include <Obol/cad/CadAssembly.h>

#include <Inventor/SoType.h>

namespace obol {
namespace {

SoCADAssembly::DrawMode toLegacyDrawMode(CadDrawMode mode)
{
    switch (mode) {
    case CadDrawMode::Shaded:
        return SoCADAssembly::SHADED;
    case CadDrawMode::Wireframe:
        return SoCADAssembly::WIREFRAME;
    case CadDrawMode::ShadedWithEdges:
        return SoCADAssembly::SHADED_WITH_EDGES;
    }
    return SoCADAssembly::WIREFRAME;
}

SoCADAssembly::PickMode toLegacyPickMode(CadPickMode mode)
{
    switch (mode) {
    case CadPickMode::Auto:
        return SoCADAssembly::PICK_AUTO;
    case CadPickMode::Edge:
        return SoCADAssembly::PICK_EDGE;
    case CadPickMode::Triangle:
        return SoCADAssembly::PICK_TRIANGLE;
    case CadPickMode::Bounds:
        return SoCADAssembly::PICK_BOUNDS;
    case CadPickMode::Hybrid:
        return SoCADAssembly::PICK_HYBRID;
    }
    return SoCADAssembly::PICK_AUTO;
}

void ensureCadNodeClassInitialized()
{
    if (SoCADAssembly::getClassTypeId().isBad()) {
        SoCADAssembly::initClass();
    }
}

} // namespace

void
CadAssembly::setDrawMode(CadDrawMode mode)
{
    drawMode_ = mode;
}

CadDrawMode
CadAssembly::drawMode() const
{
    return drawMode_;
}

void
CadAssembly::setPickMode(CadPickMode mode)
{
    pickMode_ = mode;
}

CadPickMode
CadAssembly::pickMode() const
{
    return pickMode_;
}

void
CadAssembly::setEdgePickTolerance(float pixels)
{
    edgePickTolerancePx_ = pixels;
}

float
CadAssembly::edgePickTolerance() const
{
    return edgePickTolerancePx_;
}

void
CadAssembly::setWireframeOcclusion(bool enabled)
{
    wireframeOcclusion_ = enabled;
}

bool
CadAssembly::wireframeOcclusion() const
{
    return wireframeOcclusion_;
}

void
CadAssembly::setLodEnabled(bool enabled)
{
    lodEnabled_ = enabled;
}

bool
CadAssembly::lodEnabled() const
{
    return lodEnabled_;
}

void
CadAssembly::upsertPart(PartId id, const PartGeometry & geometry)
{
    parts_[id] = geometry;
}

void
CadAssembly::removePart(PartId id)
{
    parts_.erase(id);
}

InstanceId
CadAssembly::upsertInstanceAuto(const InstanceRecord & record)
{
    const InstanceId id = CadIdBuilder::extendNameOccBool(record.parent,
                                                          record.childName,
                                                          record.occurrenceIndex,
                                                          record.boolOp);
    upsertInstance(id, record);
    return id;
}

void
CadAssembly::upsertInstance(InstanceId id, const InstanceRecord & record)
{
    instances_[id] = record;
}

void
CadAssembly::removeInstance(InstanceId id)
{
    instances_.erase(id);
}

void
CadAssembly::updateInstanceTransform(InstanceId id, const SbMatrix & localToRoot)
{
    auto it = instances_.find(id);
    if (it != instances_.end()) {
        it->second.localToRoot = localToRoot;
    }
}

void
CadAssembly::updateInstanceStyle(InstanceId id, const InstanceStyle & style)
{
    auto it = instances_.find(id);
    if (it != instances_.end()) {
        it->second.style = style;
    }
}

void
CadAssembly::setSelectedInstances(const std::vector<InstanceId> & ids)
{
    selectedInstances_ = ids;
}

void
CadAssembly::setHiddenInstances(const std::vector<InstanceId> & ids)
{
    hiddenInstances_ = ids;
}

size_t
CadAssembly::partCount() const
{
    return parts_.size();
}

size_t
CadAssembly::instanceCount() const
{
    return instances_.size();
}

const PartGeometry *
CadAssembly::partGeometry(PartId id) const
{
    auto it = parts_.find(id);
    return it == parts_.end() ? nullptr : &it->second;
}

std::optional<InstanceRecord>
CadAssembly::getInstanceRecord(InstanceId id) const
{
    auto it = instances_.find(id);
    return it == instances_.end()
        ? std::optional<InstanceRecord>{}
        : std::optional<InstanceRecord>{it->second};
}

SoCADAssembly *
CadAssembly::createLegacyNode() const
{
    ensureCadNodeClassInitialized();

    SoCADAssembly * node = new SoCADAssembly;
    node->drawMode.setValue(toLegacyDrawMode(drawMode_));
    node->pickMode.setValue(toLegacyPickMode(pickMode_));
    node->edgePickTolerancePx.setValue(edgePickTolerancePx_);
    node->wireframeOcclusion.setValue(wireframeOcclusion_ ? TRUE : FALSE);
    node->lodEnabled.setValue(lodEnabled_ ? TRUE : FALSE);

    node->beginUpdate();
    for (const auto & entry : parts_) {
        node->upsertPart(entry.first, entry.second);
    }
    for (const auto & entry : instances_) {
        node->upsertInstance(entry.first, entry.second);
    }
    node->setSelectedInstances(selectedInstances_);
    node->setHiddenInstances(hiddenInstances_);
    node->endUpdate();

    return node;
}

} // namespace obol
