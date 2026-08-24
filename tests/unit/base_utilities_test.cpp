#include <gtest/gtest.h>

#include <Inventor/SbBSPTree.h>
#include <Inventor/SbClip.h>
#include <Inventor/SbHeap.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbOctTree.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbTesselator.h>

#include <array>
#include <cmath>

namespace {

struct HeapItem {
    float priority;
    int index = -1;
};

struct TriangleCounter {
    int value = 0;
};

void countTriangle(void *, void *, void *, void * user_data)
{
    ++static_cast<TriangleCounter *>(user_data)->value;
}

SbOctTreeFuncs pointOctTreeFunctions()
{
    SbOctTreeFuncs functions;
    functions.ptinsidefunc = [](void * item, const SbVec3f & point) -> SbBool {
        return (*static_cast<SbVec3f *>(item) - point).sqrLength() < 0.01f;
    };
    functions.insideboxfunc = [](void * item, const SbBox3f & box) -> SbBool {
        return box.intersect(*static_cast<SbVec3f *>(item));
    };
    functions.insidespherefunc = [](void * item, const SbSphere & sphere) -> SbBool {
        const SbVec3f & point = *static_cast<SbVec3f *>(item);
        SbVec3f intersection;
        return sphere.intersect(SbLine(point, point + SbVec3f(0, 0, 1)), intersection);
    };
    functions.insideplanesfunc = [](void *, const SbPlane *, int) -> SbBool {
        return TRUE;
    };
    return functions;
}

TEST(BaseHeap, MaintainsPriorityAndUpdatesClientIndices)
{
    std::array<HeapItem, 5> items = {{{5.0f}, {2.0f}, {8.0f}, {1.0f}, {4.0f}}};
    SbHeapFuncs functions;
    functions.eval_func = [](void * item) -> float {
        return static_cast<HeapItem *>(item)->priority;
    };
    functions.get_index_func = [](void * item) -> int {
        return static_cast<HeapItem *>(item)->index;
    };
    functions.set_index_func = [](void * item, int index) {
        static_cast<HeapItem *>(item)->index = index;
    };

    SbHeap heap(functions, static_cast<int>(items.size()));
    for (HeapItem & item : items) heap.add(&item);
    EXPECT_EQ(heap.size(), 5);
    EXPECT_EQ(static_cast<HeapItem *>(heap.extractMin())->priority, 1.0f);
    EXPECT_EQ(static_cast<HeapItem *>(heap.extractMin())->priority, 2.0f);
    EXPECT_EQ(static_cast<HeapItem *>(heap.getMin())->priority, 4.0f);

    heap.emptyHeap();
    EXPECT_EQ(heap.size(), 0);
}

TEST(BaseTesselator, EmitsExpectedTrianglesForConvexPolygons)
{
    TriangleCounter quad_counter;
    SbTesselator tessellator(countTriangle, &quad_counter);
    const std::array<SbVec3f, 4> quad = {
        SbVec3f(-1, -1, 0), SbVec3f(1, -1, 0),
        SbVec3f(1, 1, 0), SbVec3f(-1, 1, 0)};
    tessellator.beginPolygon();
    for (const SbVec3f & vertex : quad) tessellator.addVertex(vertex, nullptr);
    tessellator.endPolygon();
    EXPECT_EQ(quad_counter.value, 2);

    TriangleCounter triangle_counter;
    SbTesselator triangle_tessellator;
    triangle_tessellator.setCallback(countTriangle, &triangle_counter);
    triangle_tessellator.beginPolygon();
    triangle_tessellator.addVertex(SbVec3f(0, 0, 0), nullptr);
    triangle_tessellator.addVertex(SbVec3f(1, 0, 0), nullptr);
    triangle_tessellator.addVertex(SbVec3f(0, 1, 0), nullptr);
    triangle_tessellator.endPolygon();
    EXPECT_EQ(triangle_counter.value, 1);

    TriangleCounter pentagon_counter;
    SbTesselator pentagon_tessellator(countTriangle, &pentagon_counter);
    std::array<SbVec3f, 5> pentagon;
    for (int index = 0; index < static_cast<int>(pentagon.size()); ++index) {
        const float angle = static_cast<float>(index) * 2.0f * 3.14159265358979323846f /
                            static_cast<float>(pentagon.size());
        pentagon[index].setValue(std::cos(angle), std::sin(angle), 0.0f);
    }
    pentagon_tessellator.beginPolygon(FALSE, SbVec3f(0.0f, 0.0f, 1.0f));
    for (SbVec3f & vertex : pentagon) pentagon_tessellator.addVertex(vertex, &vertex);
    pentagon_tessellator.endPolygon();
    EXPECT_EQ(pentagon_counter.value, 3);

    TriangleCounter retained_vertex_counter;
    SbTesselator retained_vertex_tessellator(countTriangle, &retained_vertex_counter);
    std::array<SbVec3f, 4> retained_quad = {
        SbVec3f(0, 0, 0), SbVec3f(2, 0, 0), SbVec3f(2, 2, 0), SbVec3f(0, 2, 0)};
    retained_vertex_tessellator.beginPolygon(TRUE, SbVec3f(0.0f, 0.0f, 1.0f));
    for (SbVec3f & vertex : retained_quad) retained_vertex_tessellator.addVertex(vertex, &vertex);
    retained_vertex_tessellator.endPolygon();
    EXPECT_EQ(retained_vertex_counter.value, 2);

    TriangleCounter concave_counter;
    SbTesselator concave_tessellator(countTriangle, &concave_counter);
    const std::array<SbVec3f, 6> concave = {
        SbVec3f(0, 0, 0), SbVec3f(2, 0, 0), SbVec3f(2, 1, 0),
        SbVec3f(1, 1, 0), SbVec3f(1, 2, 0), SbVec3f(0, 2, 0)};
    concave_tessellator.beginPolygon(FALSE, SbVec3f(0.0f, 0.0f, 1.0f));
    for (const SbVec3f & vertex : concave) concave_tessellator.addVertex(vertex, nullptr);
    concave_tessellator.endPolygon();
    EXPECT_EQ(concave_counter.value, 4);

    TriangleCounter repeated_polygon_counter;
    SbTesselator repeated_polygon_tessellator(countTriangle, &repeated_polygon_counter);
    repeated_polygon_tessellator.beginPolygon();
    for (const SbVec3f & vertex : std::array<SbVec3f, 3>{
             SbVec3f(0, 0, 0), SbVec3f(1, 0, 0), SbVec3f(0, 1, 0)}) {
        repeated_polygon_tessellator.addVertex(vertex, nullptr);
    }
    repeated_polygon_tessellator.endPolygon();
    repeated_polygon_tessellator.beginPolygon();
    for (const SbVec3f & vertex : quad) repeated_polygon_tessellator.addVertex(vertex, nullptr);
    repeated_polygon_tessellator.endPolygon();
    EXPECT_EQ(repeated_polygon_counter.value, 3);
}

TEST(BaseOctTree, FindsAndRemovesItemsThroughConfiguredPredicates)
{
    SbOctTree tree(SbBox3f(SbVec3f(-10, -10, -10), SbVec3f(10, 10, 10)),
                   pointOctTreeFunctions(), 4);
    std::array<SbVec3f, 4> points = {
        SbVec3f(1, 1, 1), SbVec3f(-1, -1, -1),
        SbVec3f(5, 5, 5), SbVec3f(-5, -5, -5)};
    for (SbVec3f & point : points) tree.addItem(&point);

    SbList<void *> found_at_point;
    tree.findItems(SbVec3f(1, 1, 1), found_at_point);
    ASSERT_EQ(found_at_point.getLength(), 1);
    EXPECT_EQ(found_at_point[0], &points[0]);

    SbList<void *> found_in_box;
    tree.findItems(SbBox3f(SbVec3f(0, 0, 0), SbVec3f(2, 2, 2)), found_in_box);
    ASSERT_EQ(found_in_box.getLength(), 1);
    EXPECT_EQ(found_in_box[0], &points[0]);

    tree.removeItem(&points[0]);
    SbList<void *> after_removal;
    tree.findItems(SbVec3f(1, 1, 1), after_removal);
    EXPECT_EQ(after_removal.getLength(), 0);
}

TEST(BaseBSPTree, AddsDeduplicatesAndFindsNearestPoints)
{
    SbBSPTree tree;
    const SbVec3f origin(0, 0, 0);
    const int origin_index = tree.addPoint(origin);
    const int x_index = tree.addPoint(SbVec3f(1, 0, 0));
    const int y_index = tree.addPoint(SbVec3f(0, 1, 0));

    EXPECT_GE(origin_index, 0);
    EXPECT_GE(x_index, 0);
    EXPECT_GE(y_index, 0);
    EXPECT_EQ(tree.numPoints(), 3);
    EXPECT_EQ(tree.findPoint(SbVec3f(1, 0, 0)), x_index);
    EXPECT_EQ(tree.addPoint(origin), origin_index);
    EXPECT_EQ(tree.findClosest(SbVec3f(0.1f, 0.1f, 0.0f)), origin_index);
}

TEST(BaseClip, RetainsOnlyPolygonVerticesInsideTheClippingHalfSpace)
{
    SbClip clip;
    clip.reset();
    clip.addVertex(SbVec3f(-1, 0, 0));
    clip.addVertex(SbVec3f(1, 0, 0));
    clip.addVertex(SbVec3f(1, 1, 0));
    clip.addVertex(SbVec3f(-1, 1, 0));

    clip.clip(SbPlane(SbVec3f(0, 1, 0), 0.5f));
    ASSERT_GE(clip.getNumVertices(), 2);
    for (int index = 0; index < clip.getNumVertices(); ++index) {
        SbVec3f vertex;
        clip.getVertex(index, vertex);
        EXPECT_GE(vertex[1], 0.49f);
    }
}

} // namespace
