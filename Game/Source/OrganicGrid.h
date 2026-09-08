#pragma once

#include <vector>
#include <array>
#include <span>

#include <raylib.h>


class OrganicGrid
{
public:
    struct Vertex { enum Flags { None = 0x0, Center = 0x01 }; Vector2 position; unsigned char flags; std::array<int, 6> edges; std::array<int, 6> faces; };
    struct Edge { int from; int to; std::array<int, 2> faces; };
    struct Face { std::array<int, 4> edges; std::array<int, 4> vertices; Texture2D texture; };

    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;

    OrganicGrid(int radius, float width, Vector2 center);

    void BuildRelaxed();
    void Build(float triangleBias);
    void Relax(float strength, float minEdgeLength, float maxEdgeLength);

    int EmplaceVertex(Vector2 pos, unsigned char flags = Vertex::None);
    int EmplaceEdge(int from, int to);
    int EmplaceFace(std::array<int, 4> vertexIDs, std::array<int, 4> edgeIDs);

    int FindCenterVertex(std::span<const int> vertexIDs);
    void AddFaceTexture(int id, Texture2D texture);
    int SelectFace(Vector2 world);

    bool Contains(Vector2 world);
    bool IsBorderVertex(Vertex vertex);
    bool IsBorderEdge(Edge edge);
    bool IsValidID(int id);

private:
    int radius;
    float edgeWidth;
    Vector2 position;

    void Connect(std::span<int> source, int id);
    Vector2 CentroidTriangle(Vector2 a, Vector2 b, Vector2 c);
    Vector2 CentroidPoly(std::span<const int> vertexIDs);
};
