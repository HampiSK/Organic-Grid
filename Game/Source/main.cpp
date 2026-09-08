#ifdef near
    #undef near
#endif

#ifdef far
    #undef far
#endif

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include "OrganicGrid.h"


struct TextureButton
{
    Rectangle rect;
    Texture2D texture;
};

struct Slider
{
    Rectangle track;
    float min;
    float max;
    const char *label;
    bool isInteger;
};


static int UpdateTextureButtons(std::span<const TextureButton> buttons, Vector2 mouse, bool clicked, bool &outConsumed)
{
    for (int i = 0; i < (int)buttons.size(); ++i)
    {
        if (CheckCollisionPointRec(mouse, buttons[i].rect))
        {
            outConsumed = true;
            if (clicked) return i;
            break;
        }
    }
    return -1;
}

static void DrawTextureButtons(std::span<const TextureButton> buttons, int selectedIndex)
{
    for (int i = 0; i < (int)buttons.size(); ++i)
    {
        const TextureButton &b = buttons[i];
        const Rectangle src = { 0.0f, 0.0f, (float)b.texture.width, (float)b.texture.height };
        DrawTexturePro(b.texture, src, b.rect, { 0.0f, 0.0f }, 0.0f, WHITE);
        DrawRectangleLinesEx(b.rect, selectedIndex == i ? 3.0f : 1.0f, selectedIndex == i ? YELLOW : DARKGRAY);
    }
}

static void DrawSlider(const Slider &slider, float value)
{
    const float midY = slider.track.y + slider.track.height * 0.5f;
    DrawLineEx({ slider.track.x, midY }, { slider.track.x + slider.track.width, midY }, 3.0f, DARKGRAY);

    const float t = (value - slider.min) / (slider.max - slider.min);
    const float handleX = slider.track.x + t * slider.track.width;
    DrawCircle((int)handleX, (int)midY, 4.0f, LIGHTGRAY);

    if (slider.isInteger)  DrawText(TextFormat("%s: %d", slider.label, (int)value), (int)slider.track.x, (int)(slider.track.y - 18), 16, LIGHTGRAY);
    else DrawText(TextFormat("%s: %.2f", slider.label, value), (int)slider.track.x, (int)(slider.track.y - 18), 16, LIGHTGRAY);
}

static bool UpdateSlider(const Slider &slider, Vector2 mouse, bool mouseDown, float &value, bool &outConsumed)
{
    const Rectangle hitbox = { slider.track.x - 8, slider.track.y - 8, slider.track.width + 16, slider.track.height + 16 };
    if (!CheckCollisionPointRec(mouse, hitbox)) return false;

    outConsumed = true;
    if (!mouseDown) return false;

    const float t = Clamp((mouse.x - slider.track.x) / slider.track.width, 0.0f, 1.0f);
    float newValue = slider.min + t * (slider.max - slider.min);
    if (slider.isInteger) newValue = roundf(newValue);

    if (FloatEquals(newValue, value)) return false;

    value = newValue;
    return true;
}

static void DrawKeyLegend(int screenWidth)
{
    constexpr const char *lines[] =
    {
        "Q - Relax +",
        "W - Relax -",
        "R - Rebuild",
        "Click - Place texture",
    };

    const int x = screenWidth - 220;
    int y = 16;
    for (const char *line : lines)
    {
        DrawText(line, x, y, 18, LIGHTGRAY);
        y += 20;
    }
}

static void DrawTexturePolyQuad(Texture2D texture, std::span<const Vector2> points, Color tint)
{
    constexpr std::array<Vector2, 4> texcoords =
    {
        Vector2{ 0.0f, 0.0f },
        Vector2{ 1.0f, 0.0f },
        Vector2{ 1.0f, 1.0f },
        Vector2{ 0.0f, 1.0f }
    };

    if (points.size() != 4) return;

    rlSetTexture(texture.id);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);

    rlTexCoord2f(texcoords[0].x, texcoords[0].y); rlVertex2f(points[0].x, points[0].y);
    rlTexCoord2f(texcoords[1].x, texcoords[1].y); rlVertex2f(points[1].x, points[1].y);
    rlTexCoord2f(texcoords[2].x, texcoords[2].y); rlVertex2f(points[2].x, points[2].y);

    rlTexCoord2f(texcoords[0].x, texcoords[0].y); rlVertex2f(points[0].x, points[0].y);
    rlTexCoord2f(texcoords[2].x, texcoords[2].y); rlVertex2f(points[2].x, points[2].y);
    rlTexCoord2f(texcoords[3].x, texcoords[3].y); rlVertex2f(points[3].x, points[3].y);

    rlEnd();
    rlSetTexture(0);
}

static void DrawGrid(OrganicGrid &grid)
{
    for (const OrganicGrid::Face &face : grid.faces)
    {
        if (face.texture.id != 0)
        {
            const std::array<Vector2, 4> points =
            {
                grid.vertices[face.vertices[0]].position,
                grid.vertices[face.vertices[1]].position,
                grid.vertices[face.vertices[2]].position,
                grid.vertices[face.vertices[3]].position
            };

            DrawTexturePolyQuad(face.texture, points, WHITE);
        }
        else
        {
            for (int edgeID : face.edges)
            {
                if (!grid.IsValidID(edgeID)) break;
                DrawLineV(grid.vertices[grid.edges[edgeID].from].position, grid.vertices[grid.edges[edgeID].to].position, DARKGRAY);
            }
        }
    }
}

static void DrawHoveredTile(OrganicGrid &grid, int id)
{
    if (id >= grid.faces.size() || id < 0) return;

    const int centerVertexID = grid.FindCenterVertex(grid.faces[id].vertices);
    if (!grid.IsValidID(centerVertexID)) return;

    for (int faceID : grid.vertices[centerVertexID].faces)
    {
        if (!grid.IsValidID(faceID)) break;
        for (int vertexID : grid.faces[faceID].vertices)
        {
            if (!grid.IsValidID(vertexID)) break;
            if (vertexID == centerVertexID) continue;
            DrawCircleV(grid.vertices[vertexID].position, 3, RED);
        }
    }
}

static void AddTileTexture(OrganicGrid &grid, int id, Texture2D texture)
{
    if (id < 0 || id >= grid.faces.size() || grid.vertices.empty()) return;

    const int centerVertexID = grid.FindCenterVertex(grid.faces[id].vertices);
    if (!grid.IsValidID(centerVertexID)) return;

    for (int faceID : grid.vertices[centerVertexID].faces)
    {
        if (!grid.IsValidID(faceID)) break;
        grid.faces[faceID].texture = texture;
    }
}

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Organic Grid");
    SetTargetFPS(60);

    Texture2D tile1 = LoadTexture("../Resource/tile_1.png");
    Texture2D tile2 = LoadTexture("../Resource/tile_2.png");
    Texture2D tile3 = LoadTexture("../Resource/tile_3.png");

    int radius = 3;
    float width = 50.0f;
    float triangleChance = 0.2f;
    float strength = 0.1f;

    OrganicGrid grid(radius, width, { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f });
    grid.BuildRelaxed();

    const std::array<TextureButton, 3> textureButtons =
    {
        TextureButton{ Rectangle{ GetScreenWidth() / 2.0f - 88.0f, GetScreenHeight() - 64.0f, 48.0f, 48.0f }, tile1 },
        TextureButton{ Rectangle{ GetScreenWidth() / 2.0f - 24.0f, GetScreenHeight() - 64.0f, 48.0f, 48.0f }, tile2 },
        TextureButton{ Rectangle{ GetScreenWidth() / 2.0f + 40.0f, GetScreenHeight() - 64.0f, 48.0f, 48.0f }, tile3 },
    };
    int selectedTextureIndex = 0;

    const Slider radiusSlider = { Rectangle{ (float)GetScreenWidth() - 200.0f, 120.0f, 160.0f, 4.0f }, 1.0f, 10.0f, "Radius", true };
    const Slider widthSlider = { Rectangle{ (float)GetScreenWidth() - 200.0f, 150.0f, 160.0f, 4.0f }, 1.0f, 150.0f, "Width", false };
    const Slider triangleSlider = { Rectangle{ (float)GetScreenWidth() - 200.0f, 180.0f, 160.0f, 4.0f }, 0.0f, 1.0f, "Triangle chance", false };
    const Slider strengthSlider = { Rectangle{ (float)GetScreenWidth() - 200.0f, 210.0f, 160.0f, 4.0f }, 0.0f, 1.0f, "Strength", false };

    while (!WindowShouldClose())
    {
        const Vector2 pos = GetMousePosition();
        const int hovered = grid.SelectFace(pos);

        const bool mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const bool mouseClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool uiConsumed = false;

        if (IsKeyReleased(KEY_Q)) grid.Relax(strength, width / 1.5f, width * 1.5f);
        else if (IsKeyReleased(KEY_W)) grid.Relax(-strength, width / 1.5f, width * 1.5f);
        else if (IsKeyReleased(KEY_R)) grid.BuildRelaxed();

        const int clickedButton = UpdateTextureButtons(textureButtons, pos, mouseClicked, uiConsumed);
        if (clickedButton >= 0) selectedTextureIndex = clickedButton;

        float radiusValue = (float)radius;
        const bool radiusChanged = UpdateSlider(radiusSlider, pos, mouseDown, radiusValue, uiConsumed);
        const bool widthChanged = UpdateSlider(widthSlider, pos, mouseDown, width, uiConsumed);
        const bool triangleChanged = UpdateSlider(triangleSlider, pos, mouseDown, triangleChance, uiConsumed);
        const bool strengthChanged = UpdateSlider(strengthSlider, pos, mouseDown, strength, uiConsumed);

        if (radiusChanged) radius = (int)radiusValue;
        if (radiusChanged || widthChanged || triangleChanged || strengthChanged)
        {
            grid = OrganicGrid(radius, width, { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f });
            grid.Build(triangleChance);
            grid.Relax(strength, width / 1.5f, width * 1.5f);
        }

        if (mouseClicked && !uiConsumed && grid.IsValidID(hovered))
        {
            AddTileTexture(grid, hovered, textureButtons[selectedTextureIndex].texture);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawGrid(grid);
        DrawHoveredTile(grid, hovered);
        DrawTextureButtons(textureButtons, selectedTextureIndex);

        DrawSlider(radiusSlider, (float)radius);
        DrawSlider(widthSlider, width);
        DrawSlider(triangleSlider, triangleChance);
        DrawSlider(strengthSlider, strength);

        DrawKeyLegend(GetScreenWidth());


        DrawCircleV(pos, 4, WHITE);

        EndDrawing();
    }

    UnloadTexture(tile1);
    UnloadTexture(tile2);
    UnloadTexture(tile3);

    CloseWindow();
    return 0;
}
