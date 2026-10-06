#include "assets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_IMAGES 32

static Texture2D tile_textures[MAX_TILES][MAX_TILE_TEXTURES];
static Texture2D tile_top_textures[MAX_TILES];
static Texture2D tile_base_textures[MAX_TILES];
static Texture2D item_textures[MAX_ITEMS];
static struct {
    char file[CONTENT_PATH_LEN];
    Texture2D texture;
} images[MAX_IMAGES];
static int image_count;
static char data_dir[256];

static Texture2D load(const char *dir, const char *file)
{
    if (!file[0]) return (Texture2D){ 0 };
    const char *path = TextFormat("%s/textures/%s", dir, file);
    if (!FileExists(path)) {
        fprintf(stderr, "missing texture: %s\n", path);
        return (Texture2D){ 0 };
    }
    return LoadTexture(path);
}

void assets_load(const char *dir)
{
    assets_unload();
    snprintf(data_dir, sizeof data_dir, "%s", dir);
    for (int t = 0; t < content.tile_count; t++) {
        for (int i = 0; i < content.tiles[t].texture_count; i++)
            tile_textures[t][i] = load(dir, content.tiles[t].textures[i]);
        tile_top_textures[t] = load(dir, content.tiles[t].texture_top);
        tile_base_textures[t] = load(dir, content.tiles[t].texture_base);
    }
    for (int i = 0; i < content.item_count; i++) item_textures[i] = load(dir, content.items[i].texture);
}

void assets_unload(void)
{
    for (int t = 0; t < MAX_TILES; t++) {
        if (tile_top_textures[t].id) UnloadTexture(tile_top_textures[t]);
        tile_top_textures[t] = (Texture2D){ 0 };
        if (tile_base_textures[t].id) UnloadTexture(tile_base_textures[t]);
        tile_base_textures[t] = (Texture2D){ 0 };
        for (int i = 0; i < MAX_TILE_TEXTURES; i++) {
            if (tile_textures[t][i].id) UnloadTexture(tile_textures[t][i]);
            tile_textures[t][i] = (Texture2D){ 0 };
        }
    }
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (item_textures[i].id) UnloadTexture(item_textures[i]);
        item_textures[i] = (Texture2D){ 0 };
    }
    for (int i = 0; i < image_count; i++)
        if (images[i].texture.id) UnloadTexture(images[i].texture);
    image_count = 0;
}

/* Stands in for a texture that could not be loaded. */
static void draw_placeholder(Rectangle r)
{
    DrawRectangleRec(r, MAGENTA);
    DrawRectangleRec((Rectangle){ r.x, r.y, r.width / 2, r.height / 2 }, BLACK);
    DrawRectangleRec((Rectangle){ r.x + r.width / 2, r.y + r.height / 2, r.width / 2, r.height / 2 },
                     BLACK);
}

TilePart assets_tile_part(TileType t, TileType above, TileType below)
{
    if (above != t) return TILE_PART_TOP;
    return below != t ? TILE_PART_BASE : TILE_PART_BODY;
}

void assets_draw_tile(TileType t, int x, int y, float px, float py, float tile_size, float dim, TilePart part)
{
    if (t <= TILE_AIR || t >= content.tile_count) return;
    const TileDef *def = &content.tiles[t];
    /* Cheap per-position hash: which texture, and how bright. */
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    Texture2D tex = def->texture_count > 0 ? tile_textures[t][(h >> 4) % (unsigned)def->texture_count]
                                           : (Texture2D){ 0 };
    if (part == TILE_PART_TOP && tile_top_textures[t].id) tex = tile_top_textures[t];
    if (part == TILE_PART_BASE && tile_base_textures[t].id) tex = tile_base_textures[t];
    if (!tex.id) {
        draw_placeholder((Rectangle){ px, py, tile_size, tile_size });
        return;
    }

    float light = 1.0f - dim;
    if (def->shade > 0) light *= 1.0f - (float)((h >> 8) % (unsigned)(def->shade + 1)) / 255.0f;
    if (def->pulse > 0.0f) light *= 1.0f - def->pulse * (0.5f + 0.5f * sinf((float)GetTime() * 2.5f));
    unsigned char v = (unsigned char)(255.0f * (light < 0.0f ? 0.0f : light));

    /* A texture bigger than a tile overhangs it, centred, resting on its bottom edge. */
    float scale = tile_size / TEXTURE_PIXELS_PER_TILE;
    float w = (float)tex.width * scale, hgt = (float)tex.height * scale;
    DrawTexturePro(tex, (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
                   (Rectangle){ px + (tile_size - w) / 2, py + tile_size - hgt, w, hgt },
                   (Vector2){ 0, 0 }, 0.0f, (Color){ v, v, v, 255 });
}

void assets_draw_item(ItemId id, Rectangle r)
{
    if (id <= ITEM_NONE || id >= content.item_count) return;
    Texture2D tex = item_textures[id];
    if (!tex.id) {
        draw_placeholder((Rectangle){ r.x + r.width * 0.2f, r.y + r.height * 0.2f, r.width * 0.6f,
                                      r.height * 0.6f });
        return;
    }
    DrawTexturePro(tex, (Rectangle){ 0, 0, (float)tex.width, (float)tex.height }, r,
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void assets_draw_sprite(const char *file, Rectangle r, bool flip)
{
    int found = -1;
    for (int i = 0; i < image_count; i++)
        if (strcmp(images[i].file, file) == 0) found = i;
    if (found < 0 && image_count < MAX_IMAGES) {
        found = image_count++;
        snprintf(images[found].file, sizeof images[found].file, "%s", file);
        images[found].texture = load(data_dir, file);
    }
    if (found < 0 || !images[found].texture.id) {
        draw_placeholder(r);
        return;
    }
    Texture2D tex = images[found].texture;
    DrawTexturePro(tex, (Rectangle){ 0, 0, flip ? -(float)tex.width : (float)tex.width, (float)tex.height }, r,
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void assets_draw_image(const char *file, Rectangle r)
{
    assets_draw_sprite(file, r, false);
}
