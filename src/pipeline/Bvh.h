#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace HuanGL {

// A single triangle with per-vertex normals and a material index.
// This is the leaf payload stored in the BVH; it mirrors the GLSL struct
// that the GPU path tracer will use in Task 6.
struct BvhTri {
    glm::vec3 p0, p1, p2;           // vertex positions
    glm::vec3 n0, n1, n2;           // per-vertex normals (for interpolation)
    uint32_t  materialIndex;
};

// A node in the flat BVH array.
//   triCount > 0  =>  leaf:     leftFirst = first triangle index in tris_[]
//   triCount == 0 =>  internal: leftFirst = index of left child;
//                               right child is always leftFirst + 1
struct BvhNode {
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
    int leftFirst;   // meaning depends on triCount (see above)
    int triCount;
};

class Bvh {
public:
    // Build a median-split BVH over the given triangles.
    // The tris vector is taken by value; Build reorders it in place so that
    // leaf nodes reference contiguous index ranges.
    void Build(std::vector<BvhTri> tris);

    const std::vector<BvhNode>& Nodes() const { return nodes_; }
    const std::vector<BvhTri>&  Tris()  const { return tris_;  }
    bool Empty() const { return tris_.empty(); }

private:
    std::vector<BvhNode> nodes_;
    std::vector<BvhTri>  tris_;

    // Recursively build a subtree covering tris_[first .. first+count).
    // Returns the index of the new node appended to nodes_.
    void BuildRecursive(int first, int count);
};

// CPU reference traversal — mirrors the GLSL traversal that will be written
// in Task 6.  Returns the index into bvh.Tris() of the closest hit, or -1.
// tOut receives the ray parameter t of that hit.
int BvhSelfTest_ClosestHit(const Bvh& bvh, glm::vec3 ro, glm::vec3 rd, float& tOut);

namespace BvhSelfTest {
// Run all assertion-based self-tests.  Returns true if every assert passes.
bool RunAll();
} // namespace BvhSelfTest

} // namespace HuanGL
