#include "menu.h"

#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define KEYBINDS_PATH SAVE_DIR "/keybinds.txt"
#define BUTTON_WIDTH 320.0f
#define BUTTON_HEIGHT 40.0f
#define BUTTON_GAP 10.0f
#define MAX_SLIDER_TICK_RATE 60

static const Color panel_color = { 28, 30, 38, 245 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 54, 57, 70, 255 };
static const Color button_hot = { 80, 84, 100, 255 };
static const Color accent_color = { 70, 130, 80, 255 };

/* ---- widgets ----------------------------------------------------------- */

static void draw_centered(const char *text, Rectangle r, int size, Color c)
{
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, size)) / 2),
             (int)(r.y + (r.height - size) / 2), size, c);
}

/* Draws a button and reports a click on it. */
static bool button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hot ? button_hot : button_color);
    DrawRectangleLinesEx(r, 2.0f, edge_color);
    draw_centered(text, r, 20, enabled ? RAYWHITE : GRAY);
    return hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

/* The i-th button in a centred column that starts at `top`. */
static Rectangle column_button(float top, int i)
{
    return (Rectangle){ (GetScreenWidth() - BUTTON_WIDTH) / 2,
                        top + i * (BUTTON_HEIGHT + BUTTON_GAP), BUTTON_WIDTH, BUTTON_HEIGHT };
}

static void title(const char *text, float y, int size)
{
    DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, (int)y, size, RAYWHITE);
}

/* ---- key bindings ------------------------------------------------------ */

static const char *bind_labels[BIND_COUNT] = { "Move left", "Move right", "Climb up",
                                               "Climb down", "Follow player" };
static const char *bind_names[BIND_COUNT] = { "left", "right", "up", "down", "follow" };

void keybinds_defaults(Keybinds *k)
{
    k->keys[BIND_LEFT] = KEY_A;
    k->keys[BIND_RIGHT] = KEY_D;
    k->keys[BIND_UP] = KEY_W;
    k->keys[BIND_DOWN] = KEY_S;
    k->keys[BIND_FOLLOW] = KEY_SPACE;
}

void keybinds_load(Keybinds *k)
{
    keybinds_defaults(k);
    FILE *f = fopen(KEYBINDS_PATH, "r");
    if (!f) return;
    char name[32];
    int key;
    while (fscanf(f, "%31s %d", name, &key) == 2) {
        for (int i = 0; i < BIND_COUNT; i++)
            if (strcmp(name, bind_names[i]) == 0 && key > 0 && key < 400) k->keys[i] = key;
    }
    fclose(f);
}

void keybinds_save(const Keybinds *k)
{
    if (!DirectoryExists(SAVE_DIR)) MakeDirectory(SAVE_DIR);
    FILE *f = fopen(KEYBINDS_PATH, "w");
    if (!f) return;
    for (int i = 0; i < BIND_COUNT; i++) fprintf(f, "%s %d\n", bind_names[i], k->keys[i]);
    fclose(f);
}

static const char *key_name(int key)
{
    static const struct {
        int key;
        const char *name;
    } named[] = {
        { KEY_SPACE, "Space" },      { KEY_ENTER, "Enter" },          { KEY_TAB, "Tab" },
        { KEY_BACKSPACE, "Backspace" }, { KEY_UP, "Up arrow" },       { KEY_DOWN, "Down arrow" },
        { KEY_LEFT, "Left arrow" },  { KEY_RIGHT, "Right arrow" },    { KEY_LEFT_SHIFT, "Left shift" },
        { KEY_RIGHT_SHIFT, "Right shift" }, { KEY_LEFT_CONTROL, "Left ctrl" },
        { KEY_RIGHT_CONTROL, "Right ctrl" }, { KEY_LEFT_ALT, "Left alt" },
        { KEY_RIGHT_ALT, "Right alt" },
    };
    for (size_t i = 0; i < sizeof named / sizeof named[0]; i++)
        if (named[i].key == key) return named[i].name;
    if (key > 32 && key < 127) return TextFormat("%c", key);
    return TextFormat("Key %d", key);
}

/* ---- main menu --------------------------------------------------------- */

static void world_path(char *out, size_t size, const char *name)
{
    snprintf(out, size, "%s/%s.world", SAVE_DIR, name);
}

static void begin_creating(MainMenu *m)
{
    char path[256];
    m->creating = true;
    m->error[0] = '\0';
    /* Offer the first "World N" that is not taken. */
    for (int n = 1; n < 1000; n++) {
        snprintf(m->name, sizeof m->name, "World %d", n);
        world_path(path, sizeof path, m->name);
        if (!FileExists(path)) break;
    }
}

static MainMenuResult create_screen(MainMenu *m)
{
    float top = GetScreenHeight() * 0.3f;
    title("New world", top - 100, 40);

    size_t len = strlen(m->name);
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed()) {
        bool ok = c < 128 && (isalnum(c) || c == ' ' || c == '-' || c == '_');
        if (ok && len < WORLD_NAME_MAX) {
            m->name[len++] = (char)c;
            m->name[len] = '\0';
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0)
        m->name[--len] = '\0';

    Rectangle box = column_button(top, 0);
    DrawText("Name", (int)box.x, (int)box.y - 24, 18, LIGHTGRAY);
    DrawRectangleRec(box, (Color){ 18, 19, 24, 255 });
    DrawRectangleLinesEx(box, 2.0f, RAYWHITE);
    bool blink = (int)(GetTime() * 2.0) % 2 == 0;
    DrawText(TextFormat("%s%s", m->name, blink ? "_" : ""), (int)box.x + 10, (int)box.y + 10, 20,
             RAYWHITE);
    DrawText("A new world is generated with a random seed.", (int)box.x, (int)(box.y + 52), 14, GRAY);

    while (len > 0 && m->name[len - 1] == ' ') len--;
    bool create = button(column_button(top + 90, 0), "Create", len > 0) ||
                  (len > 0 && IsKeyPressed(KEY_ENTER));
    if (button(column_button(top + 90, 1), "Back", true) || IsKeyPressed(KEY_ESCAPE))
        m->creating = false;
    if (m->error[0]) title(m->error, top + 200, 18);

    if (!create) return MAIN_MENU_STAY;
    m->name[len] = '\0';
    world_path(m->path, sizeof m->path, m->name);
    if (FileExists(m->path)) {
        snprintf(m->error, sizeof m->error, "There is already a world called \"%s\".", m->name);
        return MAIN_MENU_STAY;
    }
    return MAIN_MENU_CREATE;
}

MainMenuResult main_menu_frame(MainMenu *m)
{
    ClearBackground((Color){ 24, 28, 44, 255 });
    if (m->creating) return create_screen(m);

    MainMenuResult result = MAIN_MENU_STAY;
    title("BlockGame", 60, 56);

    /* Saved worlds, in a list that scrolls when it is longer than the window. */
    FilePathList files = { 0 };
    if (DirectoryExists(SAVE_DIR)) files = LoadDirectoryFilesEx(SAVE_DIR, ".world", false);

    float list_top = 160.0f, list_bottom = GetScreenHeight() - 150.0f;
    float row = BUTTON_HEIGHT + BUTTON_GAP;
    float max_scroll = files.count * row - (list_bottom - list_top);
    m->scroll -= GetMouseWheelMove() * 30.0f;
    if (m->scroll > max_scroll) m->scroll = max_scroll;
    if (m->scroll < 0.0f) m->scroll = 0.0f;

    if (files.count == 0) title("No worlds yet. Create one to start.", list_top + 20, 20);
    BeginScissorMode(0, (int)list_top, GetScreenWidth(), (int)(list_bottom - list_top));
    bool in_list = GetMousePosition().y >= list_top && GetMousePosition().y < list_bottom;
    for (unsigned i = 0; i < files.count; i++) {
        Rectangle r = column_button(list_top - m->scroll, (int)i);
        if (r.y + r.height < list_top || r.y > list_bottom) continue;
        if (button(r, GetFileNameWithoutExt(files.paths[i]), in_list)) {
            snprintf(m->path, sizeof m->path, "%s", files.paths[i]);
            snprintf(m->name, sizeof m->name, "%s", GetFileNameWithoutExt(files.paths[i]));
            result = MAIN_MENU_LOAD;
        }
    }
    EndScissorMode();
    UnloadDirectoryFiles(files);

    if (button(column_button(list_bottom + 20, 0), "New world", true)) begin_creating(m);
    if (button(column_button(list_bottom + 20, 1), "Quit", true)) result = MAIN_MENU_QUIT;
    if (m->error[0]) title(m->error, 126, 18);
    return result;
}

/* ---- pause menu -------------------------------------------------------- */

void pause_menu_open(PauseMenu *p)
{
    p->page = PAUSE_PAGE_MAIN;
    p->rebinding = -1;
    p->slider_held = false;
    p->just_opened = true;
}

static void world_page(PauseMenu *p, Game *g, float top)
{
    title("World settings", top - 60, 32);

    Rectangle row = column_button(top, 0);
    DrawText(TextFormat("Tick rate: %d per second", g->tick_rate), (int)row.x, (int)row.y, 20,
             RAYWHITE);

    /* Slider from 1 to 60, with a button either side for single steps. */
    Rectangle minus = { row.x, row.y + 34, 40, 36 };
    Rectangle plus = { row.x + row.width - 40, row.y + 34, 40, 36 };
    Rectangle track = { minus.x + 56, row.y + 34, row.width - 112, 36 };
    if (button(minus, "-", g->tick_rate > 1)) g->tick_rate--;
    if (button(plus, "+", g->tick_rate < MAX_SLIDER_TICK_RATE)) g->tick_rate++;

    Vector2 m = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, track))
        p->slider_held = true;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) p->slider_held = false;
    if (p->slider_held) {
        float t = (m.x - track.x) / track.width;
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
        g->tick_rate = 1 + (int)(t * (MAX_SLIDER_TICK_RATE - 1) + 0.5f);
    }
    int shown = g->tick_rate > MAX_SLIDER_TICK_RATE ? MAX_SLIDER_TICK_RATE : g->tick_rate;
    float knob = track.x + track.width * (float)(shown - 1) / (MAX_SLIDER_TICK_RATE - 1);
    float mid = track.y + track.height / 2;
    DrawRectangleRec((Rectangle){ track.x, mid - 3, track.width, 6 }, button_color);
    DrawRectangleRec((Rectangle){ track.x, mid - 3, knob - track.x, 6 }, accent_color);
    DrawRectangleRec((Rectangle){ knob - 5, mid - 12, 10, 24 }, RAYWHITE);

    float day = (float)DAY_TICKS / (float)g->tick_rate, night = (float)NIGHT_TICKS / (float)g->tick_rate;
    DrawText(TextFormat("Day: %d ticks (%.0f s)    Night: %d ticks (%.0f s)", DAY_TICKS, day,
                        NIGHT_TICKS, night),
             (int)row.x, (int)row.y + 86, 14, LIGHTGRAY);
    DrawText("Trees and fruit grow by ticks; movement does not.", (int)row.x, (int)row.y + 106, 14,
             GRAY);

    if (button(column_button(top + 150, 0), "Back", true)) p->page = PAUSE_PAGE_MAIN;
}

static void keys_page(PauseMenu *p, Keybinds *k, float top)
{
    title("Keybinds", top - 60, 32);

    if (p->rebinding >= 0) {
        int key = GetKeyPressed();
        if (key == KEY_ESCAPE) {
            p->rebinding = -1;
        } else if (key != 0) {
            /* A key already in use swaps places with the one being replaced. */
            for (int i = 0; i < BIND_COUNT; i++)
                if (k->keys[i] == key) k->keys[i] = k->keys[p->rebinding];
            k->keys[p->rebinding] = key;
            p->rebinding = -1;
            keybinds_save(k);
        }
    }

    for (int i = 0; i < BIND_COUNT; i++) {
        Rectangle row = column_button(top, i);
        Rectangle key = { row.x + row.width - 150, row.y, 150, row.height };
        DrawText(bind_labels[i], (int)row.x, (int)row.y + 10, 20, RAYWHITE);
        const char *text = p->rebinding == i ? "Press a key" : key_name(k->keys[i]);
        if (button(key, text, p->rebinding < 0)) p->rebinding = i;
    }
    if (button(column_button(top, BIND_COUNT), "Reset to defaults", p->rebinding < 0)) {
        keybinds_defaults(k);
        keybinds_save(k);
    }
    if (button(column_button(top, BIND_COUNT + 1), "Back", p->rebinding < 0))
        p->page = PAUSE_PAGE_MAIN;
}

PauseResult pause_menu_frame(PauseMenu *p, Game *g, Keybinds *k, const char *world_name)
{
    PauseResult result = PAUSE_STAY;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.55f));
    Rectangle panel = { (GetScreenWidth() - BUTTON_WIDTH) / 2 - 40, 70, BUTTON_WIDTH + 80,
                        GetScreenHeight() - 140.0f };
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 2.0f, edge_color);
    float top = panel.y + 110;

    /* Escape backs out one level; while rebinding, the keys page takes it. */
    bool escape = p->rebinding < 0 && IsKeyPressed(KEY_ESCAPE);
    /* The press that opened the menu is not also a press to close it. */
    if (p->just_opened) escape = false;
    p->just_opened = false;

    if (p->page == PAUSE_PAGE_WORLD) {
        world_page(p, g, top);
        if (escape) p->page = PAUSE_PAGE_MAIN;
    } else if (p->page == PAUSE_PAGE_KEYS) {
        keys_page(p, k, top);
        if (escape) p->page = PAUSE_PAGE_MAIN;
    } else {
        title("Paused", top - 70, 32);
        title(world_name, top - 30, 18);
        if (button(column_button(top, 0), "Resume", true) || escape) result = PAUSE_RESUME;
        if (button(column_button(top, 1), "World settings", true)) p->page = PAUSE_PAGE_WORLD;
        if (button(column_button(top, 2), "Keybinds", true)) p->page = PAUSE_PAGE_KEYS;
        if (button(column_button(top, 3), "Save and quit to menu", true)) result = PAUSE_QUIT;
    }
    return result;
}
