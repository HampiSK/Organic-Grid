#pragma once

#include <vector>
#include <array>
#include <span>

#include <raylib.h>

#include "Types.hpp"


class OrganicGrid
{
public:
    struct Vertex { enum Flags { None = 0x0, Center = 0x01 }; Vector2 position; u8 flags; std::array<i32, 6> edges; std::array<i32, 6> faces; };
    struct Edge { i32 from; i32 to; std::array<i32, 2> faces; };
    struct Face { std::array<i32, 4> edges; std::array<i32, 4> vertices; Texture2D texture; };

    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;

    OrganicGrid(i32 radius, r32 width, Vector2 center);

    void BuildRelaxed();
    void Build(r32 triangleBias);
    void Relax(r32 strength, r32 minEdgeLength, r32 maxEdgeLength);

    i32 EmplaceVertex(Vector2 pos, u8 flags = Vertex::None);
    i32 EmplaceEdge(i32 from, i32 to);
    i32 EmplaceFace(std::array<i32, 4> vertexIDs, std::array<i32, 4> edgeIDs);

    i32 FindCenterVertex(std::span<const i32> vertexIDs);
    void AddFaceTexture(i32 id, Texture2D texture);
    i32 SelectFace(Vector2 world);

    bool Contains(Vector2 world);
    bool IsBorderVertex(Vertex vertex);
    bool IsBorderEdge(Edge edge);
    bool IsValidID(i32 id);

private:
    i32 radius;
    r32 edgeWidth;
    Vector2 position;

    void Connect(std::span<i32> source, i32 id);
    Vector2 CentroidTriangle(Vector2 a, Vector2 b, Vector2 c);
    Vector2 CentroidPoly(std::span<const i32> vertexIDs);
};
