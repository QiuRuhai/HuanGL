#include "Bvh.h"
#include <algorithm>
#include <cassert>
#include <cfloat>

namespace HuanGL {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Compute the axis-aligned bounding box of a single triangle.
static void TriBounds(const BvhTri& tri,
                      glm::vec3& outMin, glm::vec3& outMax)
{
    outMin = glm::min(glm::min(tri.p0, tri.p1), tri.p2);
    outMax = glm::max(glm::max(tri.p0, tri.p1), tri.p2);
}

// Centroid of a triangle (used to decide which child a tri goes into).
static glm::vec3 TriCentroid(const BvhTri& tri)
{
    return (tri.p0 + tri.p1 + tri.p2) * (1.0f / 3.0f);
}

// ---------------------------------------------------------------------------
// Bvh::Build  (public entry point)
// ---------------------------------------------------------------------------

void Bvh::Build(std::vector<BvhTri> tris)
{
    tris_  = std::move(tris);
    nodes_.clear();

    if (tris_.empty()) return;

    // Pre-allocate a rough upper bound; actual count varies.
    nodes_.reserve(tris_.size() * 2);

    BuildRecursive(0, static_cast<int>(tris_.size()));
}

// ---------------------------------------------------------------------------
// Bvh::BuildRecursive
// ---------------------------------------------------------------------------

void Bvh::BuildRecursive(int first, int count)
{
    // --- Step 1: compute the AABB over this node's triangle range ----------
    glm::vec3 nodeMin(FLT_MAX), nodeMax(-FLT_MAX);
    for (int i = first; i < first + count; ++i) {
        glm::vec3 tMin, tMax;
        TriBounds(tris_[i], tMin, tMax);
        nodeMin = glm::min(nodeMin, tMin);
        nodeMax = glm::max(nodeMax, tMax);
    }

    // Allocate the node slot now (index is known before we recurse).
    int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.push_back(BvhNode{ nodeMin, nodeMax, first, count });

    // --- Step 2: decide leaf vs. internal -----------------------------------
    // Threshold <= 4 triangles => make a leaf (keep leftFirst = first tri idx,
    // triCount = count as set above).
    if (count <= 4) {
        // Already a valid leaf node from the push_back above.
        return;
    }

    // --- Step 3: pick the largest-extent axis for the split ----------------
    glm::vec3 extent = nodeMax - nodeMin;
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent[axis]) axis = 2;

    // --- Step 4: sort the tri range by centroid on that axis ---------------
    std::sort(tris_.begin() + first,
              tris_.begin() + first + count,
              [axis](const BvhTri& a, const BvhTri& b) {
                  return TriCentroid(a)[axis] < TriCentroid(b)[axis];
              });

    // --- Step 5: split at the median ---------------------------------------
    int leftCount  = count / 2;
    int rightFirst = first + leftCount;
    int rightCount = count - leftCount;

    // Convert this node to an internal node: triCount=0, leftFirst=left child.
    // We don't know the child index yet, so patch it after recursion.
    // Because BuildRecursive always appends, the left child will be nodeIdx+1.
    nodes_[nodeIdx].triCount  = 0;
    nodes_[nodeIdx].leftFirst = static_cast<int>(nodes_.size()); // left child next

    // --- Step 6: recurse (left child is appended first) --------------------
    BuildRecursive(first,      leftCount);
    BuildRecursive(rightFirst, rightCount);
}

// ---------------------------------------------------------------------------
// Ray-AABB slab test
// Returns true if the ray [ro, rd] intersects the AABB [bMin, bMax]
// within [tMin, tMax].  On hit, tMin/tMax are updated to the overlap.
// ---------------------------------------------------------------------------
static bool RayAabb(glm::vec3 ro, glm::vec3 invRd,
                    glm::vec3 bMin, glm::vec3 bMax,
                    float tMin, float tMax)
{
    // Slab test per axis: t at entry and exit of each slab.
    for (int i = 0; i < 3; ++i) {
        float t0 = (bMin[i] - ro[i]) * invRd[i];
        float t1 = (bMax[i] - ro[i]) * invRd[i];
        if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }
        tMin = t0 > tMin ? t0 : tMin;
        tMax = t1 < tMax ? t1 : tMax;
        if (tMax < tMin) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Möller–Trumbore ray-triangle intersection
// Returns t > 0 on hit, -1 on miss.
// ---------------------------------------------------------------------------
static float RayTri(glm::vec3 ro, glm::vec3 rd, const BvhTri& tri)
{
    const float kEps = 1e-7f;

    glm::vec3 e1 = tri.p1 - tri.p0;
    glm::vec3 e2 = tri.p2 - tri.p0;

    glm::vec3 h = glm::cross(rd, e2);
    float     a = glm::dot(e1, h);

    // Ray parallel to triangle plane.
    if (a > -kEps && a < kEps) return -1.0f;

    float     f = 1.0f / a;
    glm::vec3 s = ro - tri.p0;
    float     u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return -1.0f;

    glm::vec3 q = glm::cross(s, e1);
    float     v = f * glm::dot(rd, q);
    if (v < 0.0f || u + v > 1.0f) return -1.0f;

    float t = f * glm::dot(e2, q);
    return (t > kEps) ? t : -1.0f;
}

// ---------------------------------------------------------------------------
// BvhSelfTest_ClosestHit — stack-based BVH traversal (CPU reference)
// This function will be ported verbatim to GLSL in Task 6, so keep it
// simple and branch-light.
// ---------------------------------------------------------------------------
int BvhSelfTest_ClosestHit(const Bvh& bvh,
                            glm::vec3 ro, glm::vec3 rd,
                            float& tOut)
{
    if (bvh.Empty()) return -1;

    // Precompute reciprocal direction for slab test.
    glm::vec3 invRd = 1.0f / rd;

    int   bestTri = -1;
    float bestT   = FLT_MAX;

    // Explicit traversal stack (mirrors GLSL where recursion is forbidden).
    int  stack[64];
    int  stackTop = 0;
    stack[stackTop++] = 0; // push root

    const auto& nodes = bvh.Nodes();
    const auto& tris  = bvh.Tris();

    while (stackTop > 0) {
        const BvhNode& node = nodes[stack[--stackTop]];

        // Skip this node if the AABB is farther than the current best hit.
        if (!RayAabb(ro, invRd, node.aabbMin, node.aabbMax, 0.0f, bestT))
            continue;

        if (node.triCount > 0) {
            // Leaf: test each triangle.
            for (int i = node.leftFirst; i < node.leftFirst + node.triCount; ++i) {
                float t = RayTri(ro, rd, tris[i]);
                if (t > 0.0f && t < bestT) {
                    bestT   = t;
                    bestTri = i;
                }
            }
        } else {
            // Internal node: push both children (right first so left is
            // processed first — matches typical front-to-back order).
            stack[stackTop++] = node.leftFirst + 1; // right child
            stack[stackTop++] = node.leftFirst;     // left child
        }
    }

    tOut = bestT;
    return bestTri;
}

// ---------------------------------------------------------------------------
// BvhSelfTest::RunAll
// ---------------------------------------------------------------------------
namespace BvhSelfTest {

bool RunAll()
{
    using glm::vec3;

    // Two triangles forming a quad on the XZ plane at y=0.
    std::vector<BvhTri> tris = {
        {{-1,0,-1},{1,0,-1},{1,0,1}, {0,1,0},{0,1,0},{0,1,0}, 0},
        {{-1,0,-1},{1,0,1},{-1,0,1}, {0,1,0},{0,1,0},{0,1,0}, 0},
    };

    Bvh bvh;
    bvh.Build(tris);

    assert(!bvh.Empty());
    assert(!bvh.Nodes().empty());

    // Root AABB must enclose all geometry.
    const BvhNode& root = bvh.Nodes()[0];
    assert(root.aabbMin.x <= -1.0f && root.aabbMax.x >= 1.0f);
    assert(root.aabbMin.z <= -1.0f && root.aabbMax.z >= 1.0f);

    // A ray straight down through the center must hit at t=1.
    float t = 0.0f;
    int hit = BvhSelfTest_ClosestHit(bvh, vec3(0,1,0), vec3(0,-1,0), t);
    assert(hit >= 0 && t > 0.9f && t < 1.1f);

    // A ray pointing away (upward from y=1) must miss.
    hit = BvhSelfTest_ClosestHit(bvh, vec3(0,1,0), vec3(0,1,0), t);
    assert(hit < 0);

    return true;
}

} // namespace BvhSelfTest

} // namespace HuanGL
