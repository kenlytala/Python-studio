/* Une démo visuelle entièrement en C + raylib, sans Python. */
#include "raylib.h"
#include "graphics.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Vector2 p;
    float radius, phase;
} Star;
typedef struct {
    Vector2 p, v;
    float life;
} Particle;
static float random_float(float min, float max) {
    return min + (max - min) * (float)GetRandomValue(0, 100000) / 100000;
}
int main(int argc, char **argv) {
    int frames = 0;
    bool safe = false, msaa = false, verbose = false, software = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--safe-mode"))
            safe = true;
        else if (!strcmp(argv[i], "--msaa"))
            msaa = true;
        else if (!strcmp(argv[i], "--verbose-gl"))
            verbose = true;
        else if (!strcmp(argv[i], "--software"))
            software = true;
        else {
            fprintf(stderr, "Option inconnue : %s\n", argv[i]);
            return 2;
        }
    }
    if (software && !graphics_request_software())
        return 2;
    if (!graphics_open(960, 640, "Constellation / C + raylib", msaa && !safe, verbose))
        return PRISME_GRAPHICS_ERROR;
    SetTargetFPS(safe ? 30 : 60);
    Star stars[100];
    Particle particles[1024];
    int count = 0, frame = 0;
    for (int i = 0; i < 100; i++)
        stars[i] =
            (Star){{random_float(0, 1), random_float(0, 1)}, random_float(0.6f, 2), random_float(0, 2 * PI)};
    while (!WindowShouldClose()) {
        float dt = fminf(0.04f, GetFrameTime()), t = (float)GetTime();
        Vector2 mouse = GetMousePosition();
        int w = GetScreenWidth(), h = GetScreenHeight();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            for (int i = 0; i < 40 && count < 1024; i++) {
                float angle = random_float(0, 2 * PI), speed = random_float(30, 200);
                particles[count++] = (Particle){mouse, {cosf(angle) * speed, sinf(angle) * speed}, 1};
            }
        BeginDrawing();
        ClearBackground((Color){14, 17, 24, 255});
        DrawCircleGradient((int)mouse.x, (int)mouse.y, 200, (Color){93, 220, 163, 18},
                           (Color){14, 17, 24, 0});
        for (int i = 0; i < 100; i++) {
            Vector2 p = {stars[i].p.x * (float)w, stars[i].p.y * (float)h};
            DrawCircleV(p, stars[i].radius,
                        (Color){164, 245, 192, (unsigned char)(95 + 65 * sinf(t + stars[i].phase))});
            float distance = hypotf(p.x - mouse.x, p.y - mouse.y);
            if (distance < 140)
                DrawLineV(p, mouse, (Color){164, 245, 192, (unsigned char)((1 - distance / 140) * 85)});
        }
        for (int i = 0; i < count;) {
            Particle *p = &particles[i];
            p->p.x += p->v.x * dt;
            p->p.y += p->v.y * dt;
            p->life -= dt;
            if (p->life <= 0) {
                particles[i] = particles[--count];
                continue;
            }
            DrawCircleV(p->p, p->life * 2.5f, (Color){164, 245, 192, (unsigned char)(p->life * 255)});
            i++;
        }
        DrawText("CONSTELLATION", 36, 35, 24, (Color){230, 237, 232, 255});
        DrawText("C + raylib / Move. Click. Create.", 37, 70, 16, (Color){133, 150, 147, 255});
        DrawFPS(36, h - 37);
        EndDrawing();
        if (frames && ++frame >= frames)
            break;
    }
    CloseWindow();
    return 0;
}
