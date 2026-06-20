#include "CornellScene.h"
#include "PrimitiveMesh.h"
#include "../resource/ResourceManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace HuanGL {

// Build a quad from four coplanar vertices (CCW order when seen from the
// normal side) with a shared inward-pointing normal. Returns 4 Vertex entries
// ready to be uploaded as a pair of triangles with indices {0,1,2, 0,2,3}.
static std::vector<Vertex> MakeQuad(
    glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
    glm::vec3 normal)
{
    std::vector<Vertex> v(4);
    v[0] = { p0, normal, {0, 0}, {1, 0, 0} };
    v[1] = { p1, normal, {1, 0}, {1, 0, 0} };
    v[2] = { p2, normal, {1, 1}, {1, 0, 0} };
    v[3] = { p3, normal, {0, 1}, {1, 0, 0} };
    return v;
}

// Append all six faces of an axis-aligned box [mn, mx] to the world, each as
// a separate entity with the given material index. Normals point outward from
// the box surface (appropriate for solid inner boxes viewed from outside).
static void AppendBox(World& world, const std::string& name,
                      glm::vec3 mn, glm::vec3 mx, uint32_t matIdx)
{
    const std::vector<uint32_t> idx = { 0, 1, 2, 0, 2, 3 };

    // +Y top face: normal {0,+1,0}, CCW from above
    {
        auto& e = world.CreateEntity(name + "_top");
        auto v = MakeQuad({mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
                          {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
                          {0, 1, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, matIdx) };
    }
    // -Y bottom face: normal {0,-1,0}, CCW from below
    {
        auto& e = world.CreateEntity(name + "_bot");
        auto v = MakeQuad({mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
                          {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
                          {0, -1, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, matIdx) };
    }
    // +Z front face: normal {0,0,+1}, CCW from front
    {
        auto& e = world.CreateEntity(name + "_front");
        auto v = MakeQuad({mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                          {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
                          {0, 0, 1});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, matIdx) };
    }
    // -Z back face: normal {0,0,-1}, CCW from back
    {
        auto& e = world.CreateEntity(name + "_back");
        auto v = MakeQuad({mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z},
                          {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
                          {0, 0, -1});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, matIdx) };
    }
    // +X right face: normal {+1,0,0}, CCW from right
    {
        auto& e = world.CreateEntity(name + "_right");
        auto v = MakeQuad({mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z},
                          {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z},
                          {1, 0, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, matIdx) };
    }
    // -X left face: normal {-1,0,0}, CCW from left
    {
        auto& e = world.CreateEntity(name + "_left");
        auto v = MakeQuad({mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z},
                          {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z},
                          {-1, 0, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, matIdx) };
    }
}

void CornellScene::Init(ResourceManager& /*rm*/) {
    world_.Clear();

    // Materials: 0=white, 1=red (left wall), 2=green (right wall)
    auto& mats = world_.GetMaterials();
    mats.push_back({{},{},{},{}, {0.73f,0.73f,0.73f,1}, 1.0f, 0.0f}); // white
    mats.push_back({{},{},{},{}, {0.65f,0.05f,0.05f,1}, 1.0f, 0.0f}); // red
    mats.push_back({{},{},{},{}, {0.12f,0.45f,0.15f,1}, 1.0f, 0.0f}); // green

    // Cornell box room: interior x in [-5,5], y in [0,10], z in [-5,5].
    // Camera sits at {0,3,10} looking -Z, so it looks in through the open +Z face.
    // All wall normals point INWARD (toward the room interior).
    const std::vector<uint32_t> idx = { 0, 1, 2, 0, 2, 3 };

    // Floor (y=0): normal {0,+1,0} pointing up into the room. CCW from above.
    {
        auto& e = world_.CreateEntity("Floor");
        auto v = MakeQuad({-5, 0, -5}, { 5, 0, -5},
                          { 5, 0,  5}, {-5, 0,  5},
                          {0, 1, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, 0) };
    }

    // Ceiling (y=10): normal {0,-1,0} pointing down into the room. CCW from below.
    {
        auto& e = world_.CreateEntity("Ceiling");
        auto v = MakeQuad({-5, 10,  5}, { 5, 10,  5},
                          { 5, 10, -5}, {-5, 10, -5},
                          {0, -1, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, 0) };
    }

    // Back wall (z=-5): normal {0,0,+1} pointing toward camera. CCW from inside.
    {
        auto& e = world_.CreateEntity("BackWall");
        auto v = MakeQuad({-5, 0, -5}, { 5, 0, -5},
                          { 5,10, -5}, {-5,10, -5},
                          {0, 0, 1});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, 0) };
    }

    // Left wall (x=-5, red): normal {+1,0,0} pointing toward room interior. CCW from inside.
    {
        auto& e = world_.CreateEntity("LeftWall");
        auto v = MakeQuad({-5, 0,  5}, {-5, 0, -5},
                          {-5,10, -5}, {-5,10,  5},
                          {1, 0, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, 1) };
    }

    // Right wall (x=+5, green): normal {-1,0,0} pointing toward room interior. CCW from inside.
    {
        auto& e = world_.CreateEntity("RightWall");
        auto v = MakeQuad({ 5, 0, -5}, { 5, 0,  5},
                          { 5,10,  5}, { 5,10, -5},
                          {-1, 0, 0});
        e.meshRenderer = MeshRenderer{ BuildMesh(v, idx, 2) };
    }

    // Two white inner boxes sitting on the floor.
    // Short box: roughly 3x3x3, slightly rotated in X (no rotation applied here,
    // kept axis-aligned for simplicity; the path tracer validates GI, not exact box angle).
    AppendBox(world_, "ShortBox", {-4.0f, 0.0f, -4.5f}, {-1.0f, 3.0f, -1.5f}, 0);

    // Tall box: roughly 3x6x3
    AppendBox(world_, "TallBox", { 0.5f, 0.0f, -4.5f}, { 3.5f, 6.0f, -1.5f}, 0);

    // Soft downward sun; zero ambient so indirect bounce is the only fill.
    auto& sun = world_.GetSunLight();
    sun.direction = glm::normalize(glm::vec3(0.0f, -1.0f, -0.05f));
    sun.color     = {1.0f, 1.0f, 1.0f};
    sun.intensity = 3.0f;
    world_.GetAmbient() = {0.0f, 0.0f, 0.0f};
}

} // namespace HuanGL
