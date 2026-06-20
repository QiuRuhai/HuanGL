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

    // Allocate the root node slot first, then fill it recursively.
    // This pattern is necessary so that BuildRecursive can pre-reserve
    // contiguous child pairs before recursing into them, guaranteeing the
    // right-child == leftFirst+1 invariant required by the traversal below.
    nodes_.push_back(BvhNode{});
    BuildRecursive(0, 0, static_cast<int>(tris_.size()));
}

// ---------------------------------------------------------------------------
// Bvh::BuildRecursive
//
// Fills nodes_[nodeIdx] to cover tris_[first .. first+count).
// The slot at nodeIdx must already exist in nodes_ before this is called.
//
// Child layout guarantee: for an internal node at nodeIdx,
//   left child  = nodes_[leftFirst]
//   right child = nodes_[leftFirst + 1]
// Both slots are reserved as a contiguous pair BEFORE any recursion, so
// that recursive push_backs cannot displace them.
// ---------------------------------------------------------------------------

void Bvh::BuildRecursive(int nodeIdx, int first, int count)
{
    // --- Step 1: compute the AABB over this node's triangle range ----------
    glm::vec3 nodeMin(FLT_MAX), nodeMax(-FLT_MAX);
    for (int i = first; i < first + count; ++i) {
        glm::vec3 tMin, tMax;
        TriBounds(tris_[i], tMin, tMax);
        nodeMin = glm::min(nodeMin, tMin);
        nodeMax = glm::max(nodeMax, tMax);
    }
    nodes_[nodeIdx].aabbMin = nodeMin;
    nodes_[nodeIdx].aabbMax = nodeMax;

    // --- Step 2: decide leaf vs. internal -----------------------------------
    // Threshold <= 4 triangles => make a leaf.
    if (count <= 4) {
        nodes_[nodeIdx].leftFirst = first;
        nodes_[nodeIdx].triCount  = count;
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

    // --- Step 6: reserve the two child slots as a contiguous pair ----------
    // IMPORTANT: capture leftChildIdx into a local BEFORE recursing; the
    // recursive calls do push_backs that may reallocate nodes_, making any
    // BvhNode& dangle.  Index-based access nodes_[nodeIdx] after recursion
    // is fine — indices are stable even across reallocation.
    int leftChildIdx = static_cast<int>(nodes_.size());
    nodes_.push_back(BvhNode{});  // left  child slot
    nodes_.push_back(BvhNode{});  // right child slot  (always leftChildIdx + 1)

    // Mark this node as internal now that we know the child index.
    nodes_[nodeIdx].leftFirst = leftChildIdx;
    nodes_[nodeIdx].triCount  = 0;

    // --- Step 7: recurse into the pre-reserved slots -----------------------
    BuildRecursive(leftChildIdx,     first,      leftCount);
    BuildRecursive(leftChildIdx + 1, rightFirst, rightCount);
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

// Brute-force closest-hit: scan every triangle linearly.
// Used to cross-check BVH traversal results.
static int BruteForceClosestHit(const Bvh& bvh,
                                 glm::vec3 ro, glm::vec3 rd,
                                 float& tOut)
{
    int   bestTri = -1;
    float bestT   = FLT_MAX;
    const auto& tris = bvh.Tris();
    for (int i = 0; i < (int)tris.size(); ++i) {
        float t = RayTri(ro, rd, tris[i]);
        if (t > 0.0f && t < bestT) {
            bestT   = t;
            bestTri = i;
        }
    }
    tOut = bestT;
    return bestTri;
}

bool RunAll()
{
    using glm::vec3;

    // ------------------------------------------------------------------
    // Test 1: original two-triangle smoke test (kept verbatim)
    // ------------------------------------------------------------------
    {
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
    }

    // ------------------------------------------------------------------
    // Test 2: large grid — brute-force vs. BVH cross-check
    //
    // Build a 7x7 grid of unit quads (2 tris each) on the XZ plane at
    // y=0, giving 98 triangles.  This forces the BVH to have multiple
    // internal levels, exercising the leftFirst+1 child layout.
    //
    // For every (i,j) in a 6x6 sample grid of rays fired straight down
    // from y=2 and four diagonal rays, assert that BVH and brute-force
    // agree on hit/miss and on t within 1e-4.
    // ------------------------------------------------------------------
    {
        const int kGrid = 7;           // kGrid x kGrid quads, 2 tris each
        const float kCell = 1.0f;      // cell side length

        std::vector<BvhTri> tris;
        tris.reserve(kGrid * kGrid * 2);

        for (int gz = 0; gz < kGrid; ++gz) {
            for (int gx = 0; gx < kGrid; ++gx) {
                float x0 = gx * kCell;
                float x1 = x0 + kCell;
                float z0 = gz * kCell;
                float z1 = z0 + kCell;
                vec3 n(0, 1, 0);
                // Two triangles forming the quad
                tris.push_back({{x0,0,z0},{x1,0,z0},{x1,0,z1}, n,n,n, 0});
                tris.push_back({{x0,0,z0},{x1,0,z1},{x0,0,z1}, n,n,n, 0});
            }
        }

        assert((int)tris.size() == kGrid * kGrid * 2); // 98 triangles

        Bvh bvh;
        bvh.Build(tris);

        // The BVH must have more than one node (i.e. internal nodes exist).
        assert(bvh.Nodes().size() > 1);

        const float kEps = 1e-4f;
        float totalExtent = kGrid * kCell; // 7.0

        // --- Grid of downward rays over the plane (6x6 = 36 rays) ------
        // Rays originate at y=2, fire straight down (0,-1,0).
        // Rays inside the grid must hit at t=2; outside must miss.
        const int kRayGrid = 6;
        for (int rz = 0; rz <= kRayGrid; ++rz) {
            for (int rx = 0; rx <= kRayGrid; ++rx) {
                // Place rays at half-cell offsets so they land in triangle
                // interiors rather than on shared edges.
                float x = (rx + 0.5f) * (totalExtent / kRayGrid);
                float z = (rz + 0.5f) * (totalExtent / kRayGrid);

                vec3 ro(x, 2.0f, z);
                vec3 rd(0.0f, -1.0f, 0.0f);

                float tBvh = 0.0f, tBrute = 0.0f;
                int hitBvh   = BvhSelfTest_ClosestHit(bvh, ro, rd, tBvh);
                int hitBrute = BruteForceClosestHit(bvh, ro, rd, tBrute);

                // Hit/miss must agree.
                assert((hitBvh >= 0) == (hitBrute >= 0));
                // If both hit, t must agree.
                if (hitBvh >= 0 && hitBrute >= 0) {
                    assert(tBvh > 0.0f);
                    float diff = tBvh - tBrute;
                    if (diff < 0.0f) diff = -diff;
                    assert(diff < kEps);
                }
            }
        }

        // --- Four angled rays that cross multiple grid cells ------------
        // Origin well above the plane; non-axis-aligned direction.
        struct Ray { vec3 ro; vec3 rd; };
        Ray angledRays[] = {
            { vec3(0.5f, 5.0f, 0.5f),  glm::normalize(vec3( 1,-2, 1)) },
            { vec3(6.5f, 5.0f, 0.5f),  glm::normalize(vec3(-1,-2, 1)) },
            { vec3(0.5f, 5.0f, 6.5f),  glm::normalize(vec3( 1,-2,-1)) },
            { vec3(6.5f, 5.0f, 6.5f),  glm::normalize(vec3(-1,-2,-1)) },
            // A ray that deliberately misses (fires upward).
            { vec3(3.5f, 2.0f, 3.5f),  vec3(0, 1, 0) },
        };

        for (const Ray& r : angledRays) {
            float tBvh = 0.0f, tBrute = 0.0f;
            int hitBvh   = BvhSelfTest_ClosestHit(bvh, r.ro, r.rd, tBvh);
            int hitBrute = BruteForceClosestHit(bvh, r.ro, r.rd, tBrute);

            assert((hitBvh >= 0) == (hitBrute >= 0));
            if (hitBvh >= 0 && hitBrute >= 0) {
                assert(tBvh > 0.0f);
                float diff = tBvh - tBrute;
                if (diff < 0.0f) diff = -diff;
                assert(diff < kEps);
            }
        }
    }

    return true;
}

} // namespace BvhSelfTest

} // namespace HuanGL



