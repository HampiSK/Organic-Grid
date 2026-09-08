#include "OrganicGrid.h"

#include <cassert>
#include <ranges>
#include <algorithm>

#include <cmath>

#ifdef near
    #undef near
#endif

#ifdef far
    #undef far
#endif

#include <raymath.h>
#include <rlgl.h>

constexpr float SQRT3 = 1.7320508f;


OrganicGrid::OrganicGrid(int radius, float width, Vector2 position) :
    radius(radius), edgeWidth(width), position(position) {}

void OrganicGrid::BuildRelaxed()
{
    Build(0.2f);
    Relax(0.3f, edgeWidth / 1.5f, edgeWidth * 1.5f);
}

void OrganicGrid::Build(float triangleBias)
{
    // Available tile shapes from which they are split into faces
    enum Shape { None, CubeLeft, CubeRight, TriangleUp, TriangleDown };

    vertices.clear();
    edges.clear();
    faces.clear();

    // Assuming max, couldn't figure out proper formula
    const int maxTriangles = radius * radius * 6;
    const int maxFaces = maxTriangles * 3;
    const int maxEdges = maxFaces * 2;
    const int maxVertices = maxEdges * 6;

    vertices.reserve(maxVertices);
    edges.reserve(maxEdges);
    faces.reserve(maxFaces);

    const float horizontal = 1.5f * edgeWidth;
    const float vertical = SQRT3 * edgeWidth;

    // It was simpler to keep all vectors required to construct a shape directly
    // rather then accessing them by index from the lookup buffers
    constexpr Vector2 INVALID = { -1, -1 };
    Vector2 qTopFirst = INVALID;
    Vector2 qTopLast = INVALID;

    Vector2 qTop = INVALID;
    Vector2 qTopRight = INVALID;
    Vector2 qBottom = INVALID;
    Vector2 qBottomLeft = INVALID;

    for (int r = -radius; r <= radius; ++r)
    {
        // Give the expressions meaningful names before things get ugly (Dont worry, they will...)

        const bool isFirstRow = r == -radius;
        const bool isUpperHalfRow = r <= 0;
        const bool isLowerHalfRow = r > 0;

        const int qFirst = std::max(-radius, -r - radius);
        const int qLast = std::min(radius, -r + radius);
        int q = isFirstRow ? qLast + 1 : qFirst; // Skip first row

        const Vector2 qBottomFirst = { position.x + horizontal * qFirst, position.y + vertical * (r + qFirst * 0.5f) };
        const Vector2 qBottomLast = { position.x + horizontal * qLast, position.y + vertical * (r + qLast * 0.5f) };

        Shape previousShape = Shape::None;

        while (q <= qLast)
        {
            const bool isFirstQ = q == qFirst;
            const bool isLastQ = q == qLast;

            const bool isFirstTop = Vector2Equals(qTop, qTopFirst);
            const bool isLastTop = Vector2Equals(qTop, qTopLast);

            const bool isFirstVertex = isFirstQ && isFirstTop;
            const bool isLastVertex = isLastQ && isLastTop;

            // End reached, nothing to add
            if ((!isFirstRow && isUpperHalfRow && isLastVertex) || (isLowerHalfRow && isLastVertex)) break;

            // Each shape is responsible for updating the vertices used during shape
            // construction, incrementing the q (column), and updating the top and
            // bottom vertices where the required edge must be prepared for the next
            // edge attachment
            //
            // O    <- I.e.: We got the following edge, so the only shapes
            //  \            that can be attached here are CubeLeft or TriangleDown
            //   O           Note that in addition, we are keeping top right and bottom left 2D vectors
            //
            // O - - X       After shape is build, previous vertices (O) are updated with new (X)
            //  \     \
            //   O - - X

            constexpr int prioritySize = 4;
            constexpr Shape cubePriority[prioritySize] = { Shape::CubeLeft, Shape::CubeRight, Shape::TriangleUp, Shape::TriangleDown };
            constexpr Shape trianglePriority[prioritySize] = { Shape::TriangleUp, Shape::TriangleDown, Shape::CubeLeft, Shape::CubeRight };
            const Shape *priority = (GetRandomValue(1, 100) / 100.0f <= triangleBias) ? trianglePriority : cubePriority;

            Shape currentShape = Shape::None;
            for (int i = 0; i < prioritySize; ++i)
            {
                currentShape = priority[i];
                switch (currentShape)
                {
                    case Shape::CubeLeft:
                    {
                        if (previousShape == Shape::TriangleUp || previousShape == Shape::CubeRight) continue; // Cannot attach
                        if (isLastTop) continue; // Cannot create shape based on previous row
                        if (isUpperHalfRow && isLastVertex) continue; // Cannot be last in upper half
                        if (isLowerHalfRow && isFirstVertex) continue; // Cannot be first in bottom half

                        qBottomLeft = (isUpperHalfRow && isFirstVertex) ? qBottomFirst : qBottom;
                        qBottom = { position.x + horizontal * (q + 1), position.y + vertical * (r + (q + 1) * 0.5f) };

                        const int topLeft = EmplaceVertex(qTop);
                        const int top = EmplaceVertex(Vector2Lerp(qTop, qTopRight, 0.5f));
                        const int topRight = EmplaceVertex(qTopRight);
                        const int middleLeft = EmplaceVertex(Vector2Lerp(qBottomLeft, qTop, 0.5f));
                        const int middle = EmplaceVertex(Vector2Lerp(qBottomLeft, qTopRight, 0.5f), Vertex::Center);
                        const int middleRight = EmplaceVertex(Vector2Lerp(qBottom, qTopRight, 0.5f));
                        const int bottomLeft = EmplaceVertex(qBottomLeft);
                        const int bottom = EmplaceVertex(Vector2Lerp(qBottom, qBottomLeft, 0.5f));
                        const int bottomRight = EmplaceVertex(qBottom);

                        const int topLeftToTop = EmplaceEdge(topLeft, top);
                        const int topRightToTop = EmplaceEdge(topRight, top);
                        const int topToMiddle = EmplaceEdge(top, middle);
                        const int middleLeftToTopLeft = EmplaceEdge(middleLeft, topLeft);
                        const int middleLeftToBottomLeft = EmplaceEdge(middleLeft, bottomLeft);
                        const int middleRightToTopRight = EmplaceEdge(middleRight, topRight);
                        const int middleRightToBottomRight = EmplaceEdge(middleRight, bottomRight);
                        const int middleToMiddleLeft = EmplaceEdge(middle, middleLeft);
                        const int middleToMiddleRight = EmplaceEdge(middle, middleRight);
                        const int bottomLeftToBottom = EmplaceEdge(bottomLeft, bottom);
                        const int bottomRightToBottom = EmplaceEdge(bottomRight, bottom);
                        const int bottomToMiddle = EmplaceEdge(bottom, middle);

                        // Vertices are sorted around their center in counter-clockwise order
                        EmplaceFace({ middle, top, topLeft, middleLeft }, { middleToMiddleLeft, middleLeftToTopLeft, topLeftToTop, topToMiddle });
                        EmplaceFace({ middle, middleLeft, bottomLeft, bottom }, { middleToMiddleLeft, middleLeftToBottomLeft, bottomLeftToBottom, bottomToMiddle });
                        EmplaceFace({ middle, middleRight, topRight, top }, { middleToMiddleRight, middleRightToTopRight, topRightToTop, topToMiddle });
                        EmplaceFace({ middle, bottom, bottomRight, middleRight }, { bottomToMiddle, bottomRightToBottom, middleRightToBottomRight, middleToMiddleRight });

                        qTop = qTopRight;
                        if (!Vector2Equals(qTopRight, qTopLast)) qTopRight = { position.x + horizontal * (q + 3), position.y + vertical * (r - 1 + (q + 3) * 0.5f) };
                        ++q;
                    }
                    break;
                    case Shape::CubeRight:
                    {
                        if (previousShape == Shape::TriangleDown || previousShape == Shape::CubeLeft) continue; // Cannot attach
                        if (isLastTop) continue; // Cannot create shape based on previous row
                        if (isUpperHalfRow && isFirstVertex) continue; // Cannot be first in upper half
                        if (isLowerHalfRow && Vector2Equals(qBottom, qBottomLast)) continue; // Cannot be last in bottom half

                        qBottomLeft = (isLowerHalfRow && isFirstVertex) ? qBottomFirst : qBottom;
                        qBottom = { position.x + horizontal * (q + 1), position.y + vertical * (r + (q + 1) * 0.5f) };

                        const int topLeft = EmplaceVertex(qTop);
                        const int top = EmplaceVertex(Vector2Lerp(qTop, qTopRight, 0.5f));
                        const int topRight = EmplaceVertex(qTopRight);
                        const int middleLeft = EmplaceVertex(Vector2Lerp(qBottomLeft, qTop, 0.5f));
                        const int middle = EmplaceVertex(Vector2Lerp(qBottom, qTop, 0.5f), Vertex::Center);
                        const int middleRight = EmplaceVertex(Vector2Lerp(qBottom, qTopRight, 0.5f));
                        const int bottomLeft = EmplaceVertex(qBottomLeft);
                        const int bottom = EmplaceVertex(Vector2Lerp(qBottom, qBottomLeft, 0.5f));
                        const int bottomRight = EmplaceVertex(qBottom);

                        const int topLeftToTop = EmplaceEdge(topLeft, top);
                        const int topRightToTop = EmplaceEdge(topRight, top);
                        const int topToMiddle = EmplaceEdge(top, middle);
                        const int middleLeftToTopLeft = EmplaceEdge(middleLeft, topLeft);
                        const int middleLeftToBottomLeft = EmplaceEdge(middleLeft, bottomLeft);
                        const int middleRightToTopRight = EmplaceEdge(middleRight, topRight);
                        const int middleRightToBottomRight = EmplaceEdge(middleRight, bottomRight);
                        const int middleToMiddleLeft = EmplaceEdge(middle, middleLeft);
                        const int middleToMiddleRight = EmplaceEdge(middle, middleRight);
                        const int bottomLeftToBottom = EmplaceEdge(bottomLeft, bottom);
                        const int bottomRightToBottom = EmplaceEdge(bottomRight, bottom);
                        const int bottomToMiddle = EmplaceEdge(bottom, middle);

                        // Vertices are sorted around their center in counter-clockwise order
                        EmplaceFace({ middle, top, topLeft, middleLeft }, { middleToMiddleLeft, middleLeftToTopLeft, topLeftToTop, topToMiddle });
                        EmplaceFace({ middle, middleLeft, bottomLeft, bottom }, { middleToMiddleLeft, middleLeftToBottomLeft, bottomLeftToBottom, bottomToMiddle });
                        EmplaceFace({ middle, middleRight, topRight, top }, { middleToMiddleRight, middleRightToTopRight, topRightToTop, topToMiddle });
                        EmplaceFace({ middle, bottom, bottomRight, middleRight }, { bottomToMiddle, bottomRightToBottom, middleRightToBottomRight, middleToMiddleRight });

                        qTop = qTopRight;
                        if (!Vector2Equals(qTopRight, qTopLast)) qTopRight = { position.x + horizontal * (q + 2), position.y + vertical * (r - 1 + (q + 2) * 0.5f) };
                        ++q;
                    }
                    break;
                    case Shape::TriangleUp:
                    {
                        if (previousShape == Shape::TriangleUp || previousShape == Shape::CubeRight) continue; // Cannot connect
                        if (isLowerHalfRow && isLastVertex) continue; // Cannot be last in bottom half
                        if (isLowerHalfRow && isFirstVertex) continue; // Cannot be first in bottom half

                        qBottomLeft = (isUpperHalfRow && isFirstVertex) ? qBottomFirst : qBottom;
                        qBottom = { position.x + horizontal * (q + 1), position.y + vertical * (r + (q + 1) * 0.5f) };

                        const int top = EmplaceVertex(qTop);
                        const int middleLeft = EmplaceVertex(Vector2Lerp(qBottomLeft, qTop, 0.5f));
                        const int middle = EmplaceVertex(CentroidTriangle(qBottomLeft, qBottom, qTop), Vertex::Center);
                        const int middleRight = EmplaceVertex(Vector2Lerp(qBottom, qTop, 0.5f));
                        const int bottomLeft = EmplaceVertex(qBottomLeft);
                        const int bottom = EmplaceVertex(Vector2Lerp(qBottom, qBottomLeft, 0.5f));
                        const int bottomRight = EmplaceVertex(qBottom);

                        const int topToMiddleRight = EmplaceEdge(top, middleRight);
                        const int middleLeftToTop = EmplaceEdge(middleLeft, top);
                        const int middleLeftToBottomLeft = EmplaceEdge(middleLeft, bottomLeft);
                        const int middleRightToBottomRight = EmplaceEdge(middleRight, bottomRight);
                        const int middleToMiddleLeft = EmplaceEdge(middle, middleLeft);
                        const int middleToMiddleRight = EmplaceEdge(middle, middleRight);
                        const int bottomLeftToBottom = EmplaceEdge(bottomLeft, bottom);
                        const int bottomRightToBottom = EmplaceEdge(bottomRight, bottom);
                        const int bottomToMiddle = EmplaceEdge(bottom, middle);

                        EmplaceFace({ middleRight, top, middleLeft, middle }, { middleToMiddleRight, topToMiddleRight, middleLeftToTop, middleToMiddleLeft });
                        EmplaceFace({ bottom, middle, middleLeft, bottomLeft }, { middleToMiddleLeft, middleLeftToBottomLeft, bottomLeftToBottom, bottomToMiddle });
                        EmplaceFace({ bottom, bottomRight, middleRight, middle }, { bottomToMiddle, bottomRightToBottom, middleRightToBottomRight, middleToMiddleRight });

                        ++q;
                    }
                    break;
                    case Shape::TriangleDown:
                    {
                        if (previousShape == Shape::TriangleDown || previousShape == Shape::CubeLeft) continue; // Cannot connect
                        if (isUpperHalfRow && isFirstVertex) continue; // Cannot be first in upper half
                        if (isUpperHalfRow && isLastVertex) continue; // Cannot be last in upper half

                        if (isLowerHalfRow && isFirstVertex)
                        {
                            qBottomLeft = qBottom;
                            qBottom = { position.x + horizontal * q, position.y + vertical * (r + q * 0.5f) };
                        }

                        const int topLeft = EmplaceVertex(qTop);
                        const int top = EmplaceVertex(Vector2Lerp(qTop, qTopRight, 0.5f));
                        const int topRight = EmplaceVertex(qTopRight);
                        const int middleLeft = EmplaceVertex(Vector2Lerp(qBottom, qTop, 0.5f));
                        const int middle = EmplaceVertex(CentroidTriangle(qBottom, qTop, qTopRight), Vertex::Center);
                        const int middleRight = EmplaceVertex(Vector2Lerp(qBottom, qTopRight, 0.5f));
                        const int bottom = EmplaceVertex(qBottom);

                        const int topLeftToTop = EmplaceEdge(topLeft, top);
                        const int topRightToTop = EmplaceEdge(topRight, top);
                        const int topToMiddle = EmplaceEdge(top, middle);
                        const int middleLeftToTopLeft = EmplaceEdge(middleLeft, topLeft);
                        const int middleLeftToBottom = EmplaceEdge(middleLeft, bottom);
                        const int middleRightToMiddle = EmplaceEdge(middleRight, middle);
                        const int middleRightToTopRight = EmplaceEdge(middleRight, topRight);
                        const int middleToMiddleLeft = EmplaceEdge(middle, middleLeft);
                        const int middleToMiddleRight = EmplaceEdge(middle, middleRight);
                        const int bottomToMiddleRight = EmplaceEdge(bottom, middleRight);

                        // Vertices are sorted around their center in counter-clockwise order
                        EmplaceFace({ top, topLeft, middleLeft, middle }, { topToMiddle, topLeftToTop, middleLeftToTopLeft, middleToMiddleLeft });
                        EmplaceFace({ middleRight, middle, middleLeft, bottom }, { middleToMiddleLeft, middleLeftToBottom, bottomToMiddleRight, middleRightToMiddle });
                        EmplaceFace({ top, middle, middleRight, topRight }, { middleToMiddleRight, middleRightToTopRight, topRightToTop, topToMiddle });

                        qTop = qTopRight;
                        qTopRight = { position.x + horizontal * (q + 2), position.y + vertical * (r - 1 + (q + 2) * 0.5f) };
                    }
                    break;

                    case Shape::None:
                    default: assert(false); // Something is off
                }

                previousShape = currentShape;
                break;
            }
        }

        // Weeeeee
        qTop = qBottomFirst;
        qTopRight = { position.x + horizontal * (qFirst + 1), position.y + vertical * (r + (qFirst + 1) * 0.5f) };
        qTopFirst = qBottomFirst;
        qTopLast = qBottomLast;
    }

    vertices.shrink_to_fit();
    edges.shrink_to_fit();
    faces.shrink_to_fit();
}

void OrganicGrid::Relax(float strength, float minEdgeLength, float maxEdgeLength)
{
    // Relaxeing the grid by moving non-border edge endpoints toward each other
    // to adjust edge lengths toward the configured range

    for (const Edge &edge : edges)
    {
        Vertex &from = vertices[edge.from];
        Vertex &to = vertices[edge.to];
        if (IsBorderVertex(from) || IsBorderVertex(to)) continue;

        const Vector2 diff = Vector2Subtract(to.position, from.position);
        const float length = Vector2Length(diff);
        if (length < minEdgeLength || length > maxEdgeLength || FloatEquals(length, 0.0f)) continue;

        // Move each endpoint toward the midpoint
        const float excess = length - maxEdgeLength;
        const Vector2 dir = Vector2Scale(diff, 1.0f / length);
        const Vector2 move = Vector2Scale(dir, excess * 0.5f * strength);

        const Vector2 newFrom = Vector2Add(from.position, move);
        const Vector2 newTo = Vector2Subtract(to.position, move);

        if (Contains(newFrom)) from.position = newFrom;
        if (Contains(newTo)) to.position = newTo;
    }
}

int OrganicGrid::EmplaceVertex(Vector2 pos, unsigned char flags)
{
    // Trivially checking for duplicates
    for (int i = 0; i < vertices.size(); ++i)
    {
        if (Vector2Equals(vertices[i].position, pos)) return i;
    }

    constexpr std::array<int, 6> NO_CONNECTIONS = { -1, -1, -1, -1, -1, -1 };
    vertices.emplace_back( Vertex{ pos, flags, NO_CONNECTIONS, NO_CONNECTIONS });

    return (int)vertices.size() - 1;
}

int OrganicGrid::EmplaceEdge(int from, int to)
{
    // Trivially checking for duplicates
    for (int i = 0; i < edges.size(); ++i)
    {
        if ((edges[i].from == from && edges[i].to == to) || (edges[i].from == to && edges[i].to == from)) return i;
    }

    edges.emplace_back(Edge{ from, to, { -1, -1 } });
    const int id = (int)edges.size() - 1;

    Connect(vertices[from].edges, id);
    Connect(vertices[to].edges, id);
    return id;
}

int OrganicGrid::EmplaceFace(std::array<int, 4> vertexIDs, std::array<int, 4> edgeIDs)
{
     // Trivially checking for duplicates
    for (int i = 0; i < faces.size(); ++i)
    {
        const Face &face = faces[i];
        bool isSame = true;

        for (int j = 0; j < 4; ++j)
        {
            if (std::ranges::find(vertexIDs, face.vertices[j]) == vertexIDs.end() ||
                std::ranges::find(edgeIDs, face.edges[j]) == edgeIDs.end())
            {
                isSame = false;
                break;
            }
        }

        if (isSame) return i;
    }

    constexpr Texture2D NO_TEXTURE = { 0 };
    faces.emplace_back(Face{ edgeIDs, vertexIDs, NO_TEXTURE });
    const int id = (int)faces.size() - 1;

    for (int vertexID : vertexIDs)
    {
        Vertex &vertex = vertices[vertexID];
        Connect(vertex.faces, id);
    }

    for (int edgeID : edgeIDs)
    {
        Edge &edge = edges[edgeID];
        Connect(edge.faces, id);
    }

    return id;
}

void OrganicGrid::Connect(std::span<int> source, int id)
{
    if (!IsValidID(id)) return;
    for (int i = 0; i < source.size(); ++i)
    {
        if (source[i] == id) return;
        if (!IsValidID(source[i])) { source[i] = id; return; }
    }
}

int OrganicGrid::SelectFace(Vector2 world)
{
    if (!Contains(world)) return -1;

    // Brute forced... can be improved in future
    for (int i = 0; i < faces.size(); ++i)
    {
        Face face = faces[i];
        if (face.vertices.empty()) continue;

        // Based on CheckCollisionPointPoly
        bool collision = false;
        for (int j = 0; j < face.vertices.size(); ++j)
        {
            if (!IsValidID(face.vertices[j])) break;

            const int next = (j + 1) % face.vertices.size();
            const Vector2 vc = vertices[face.vertices[j]].position;
            const Vector2 vn = vertices[face.vertices[next]].position;

            if ((((vc.y >= world.y) && (vn.y < world.y)) || ((vc.y < world.y) && (vn.y >= world.y))) &&
                (world.x < ((vn.x - vc.x) * (world.y - vc.y) / (vn.y - vc.y) + vc.x))) collision = !collision;
        }

        if (collision) return i;
    }

    assert(false); // Should never happen
    return -1;
}

void OrganicGrid::AddFaceTexture(int id, Texture2D texture)
{
    if (id < 0 || id >= faces.size() || texture.id == 0) return;
    faces[id].texture = texture;
}

bool OrganicGrid::Contains(Vector2 world)
{
    const float horizontal = 1.5f * edgeWidth;
    const float vertical = SQRT3 * edgeWidth;

    const Vector2 topFirst = { position.x + horizontal * 0, position.y + vertical * (-radius + 0 * 0.5f) };
    const Vector2 topLast = { position.x + horizontal * radius, position.y + vertical * (-radius + radius * 0.5f) };
    const Vector2 middleFirst = { position.x + horizontal * -radius, position.y + vertical * (-radius * 0.5f) };
    const Vector2 middleLast = { position.x + horizontal * radius, position.y + vertical * (radius * 0.5f) };
    const Vector2 bottomFirst = { position.x + horizontal * -radius, position.y + vertical * (radius + -radius * 0.5f) };
    const Vector2 bottomLast = { position.x + horizontal * 0, position.y + vertical * (radius + 0 * 0.5f) };

    Vector2 perimeter[7] = { topFirst, topLast, middleLast, bottomLast, bottomFirst,  middleFirst, topFirst };
    return CheckCollisionPointPoly(world, perimeter, sizeof(perimeter) / sizeof(Vector2));
}

int OrganicGrid::FindCenterVertex(std::span<const int> vertexIDs)
{
    for (int vertexID : vertexIDs)
    {
        if (!IsValidID(vertexID)) break;
        if (vertices[vertexID].flags & Vertex::Flags::Center) return vertexID;
    }
    return -1;
}

Vector2 OrganicGrid::CentroidTriangle(Vector2 a, Vector2 b, Vector2 c)
{
    return Vector2Scale(Vector2Add(Vector2Add(a, b), c), 1.0f / 3.0f);
}

Vector2 OrganicGrid::CentroidPoly(std::span<const int> vertexIDs)
{
    float area = 0.0f;
    Vector2 centroid = Vector2Zero();

    for (int i = 0; i < vertexIDs.size(); ++i)
    {
        const Vector2 p0 = vertices[vertexIDs[i]].position;
        const Vector2 p1 = vertices[vertexIDs[(i + 1) % vertexIDs.size()]].position;

        const float cross = p0.x * p1.y - p1.x * p0.y;

        area += cross;
        centroid.x += (p0.x + p1.x) * cross;
        centroid.y += (p0.y + p1.y) * cross;
    }

    area *= 0.5f;
    if (!FloatEquals(area, 0.0f)) return Vector2Scale(centroid, 1.0f / (6.0f * area));

    // Degenerate polygon
    centroid = Vector2Zero();
    for (int i = 0; i < vertexIDs.size(); ++i)
    {
        centroid = Vector2Add(centroid, vertices[vertexIDs[i]].position);
    }
    return Vector2Scale(centroid, 1.0f / vertexIDs.size());
}

bool OrganicGrid::IsBorderVertex(Vertex vertex)
{
    int count = 0;
    for (int edgeID : vertex.edges)
    {
        if (count > 2) return false;
        if (!IsValidID(edgeID)) break;
        count += IsBorderEdge(edges[edgeID]);
    }

    return count == 2;
}

bool OrganicGrid::IsBorderEdge(Edge edge)
{
    return IsValidID(edge.faces[0]) && !IsValidID(edge.faces[1]);
}

bool OrganicGrid::IsValidID(int id)
{
    return id > -1;
}
