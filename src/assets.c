#include "assets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_IMAGES 512
#define ITEM_FILL 0.9f /* how much of its rectangle the longer side of an item takes up */

static Texture2D tile_textures[MAX_TILES][MAX_TILE_TEXTURES];
static Texture2D tile_top_textures[MAX_TILES];
static Texture2D tile_base_textures[MAX_TILES];
static Texture2D tile_lit_textures[MAX_TILES][MAX_TILE_TEXTURES];
static Texture2D tile_joined_textures[MAX_TILES];
static Texture2D tile_season_textures[MAX_TILES][MAX_SEASONS][MAX_TILE_TEXTURES];
static int season_now;
static Texture2D item_textures[MAX_ITEMS];
static Rectangle item_bounds[MAX_ITEMS]; /* the part of each item's image that is drawn on */
static Color tile_colors[MAX_TILES];
static struct {
    char file[CONTENT_PATH_LEN];
    Texture2D texture;
} images[MAX_IMAGES];
static int image_count;
static char data_dir[256];
/* Where else textures are looked for, before the game's own: the mods that are on. A later one comes first. */
static char more_dirs[64][256];
static int more_count;

/* Where a texture is: in the last of those that has it, or else in `dir`. */
static const char *find_file(const char *dir, const char *file)
{
    for (int i = more_count - 1; i >= 0; i--) {
        const char *path = TextFormat("%s/textures/%s", more_dirs[i], file);
        if (FileExists(path)) return path;
    }
    return TextFormat("%s/textures/%s", dir, file);
}

static Texture2D load(const char *dir, const char *file)
{
    if (!file[0]) return (Texture2D){ 0 };
    const char *path = find_file(dir, file);
    if (!FileExists(path)) {
        fprintf(stderr, "missing texture: %s\n", path);
        return (Texture2D){ 0 };
    }
    return LoadTexture(path);
}

/* The average of a texture's solid pixels. */
static Color average_color(const char *dir, const char *file)
{
    const char *path = find_file(dir, file);
    if (!file[0] || !FileExists(path)) return MAGENTA;
    Image img = LoadImage(path);
    long r = 0, g = 0, b = 0, n = 0;
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            Color c = GetImageColor(img, x, y);
            if (c.a < 128) continue;
            r += c.r, g += c.g, b += c.b, n++;
        }
    }
    UnloadImage(img);
    if (n == 0) return BLANK;
    return (Color){ (unsigned char)(r / n), (unsigned char)(g / n), (unsigned char)(b / n), 255 };
}

/* The box round the solid pixels of an image; the whole image if it has none. */
static Rectangle solid_bounds(const char *dir, const char *file)
{
    const char *path = find_file(dir, file);
    if (!file[0] || !FileExists(path)) return (Rectangle){ 0 };
    Image img = LoadImage(path);
    int x0 = img.width, y0 = img.height, x1 = -1, y1 = -1;
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            if (GetImageColor(img, x, y).a < 40) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }
    Rectangle all = { 0, 0, (float)img.width, (float)img.height };
    UnloadImage(img);
    if (x1 < 0) return all;
    return (Rectangle){ (float)x0, (float)y0, (float)(x1 - x0 + 1), (float)(y1 - y0 + 1) };
}

Color assets_tile_color(TileType t)
{
    return t > TILE_AIR && t < content.tile_count ? tile_colors[t] : BLANK;
}

void assets_load_from(const char *const *dirs, int count)
{
    more_count = 0;
    for (int i = 1; i < count && more_count < 64; i++) snprintf(more_dirs[more_count++], sizeof more_dirs[0], "%s", dirs[i]);
    if (count > 0) assets_load(dirs[0]);
}

void assets_load(const char *dir)
{
    assets_unload();
    snprintf(data_dir, sizeof data_dir, "%s", dir);
    for (int t = 0; t < content.tile_count; t++) {
        for (int i = 0; i < content.tiles[t].texture_count; i++)
            tile_textures[t][i] = load(dir, content.tiles[t].textures[i]);
        for (int i = 0; i < content.tiles[t].lit_count; i++)
            tile_lit_textures[t][i] = load(dir, content.tiles[t].textures_lit[i]);
        for (int k = 0; k < MAX_SEASONS; k++)
            for (int i = 0; i < content.tiles[t].season_texture_count[k]; i++)
                tile_season_textures[t][k][i] = load(dir, content.tiles[t].season_textures[k][i]);
        tile_top_textures[t] = load(dir, content.tiles[t].texture_top);
        tile_joined_textures[t] = load(dir, content.tiles[t].texture_joined);
        tile_colors[t] = average_color(dir, content.tiles[t].texture_count > 0 ? content.tiles[t].textures[0] : "");
        tile_base_textures[t] = load(dir, content.tiles[t].texture_base);
    }
    for (int i = 0; i < content.item_count; i++) {
        item_textures[i] = load(dir, content.items[i].texture);
        item_bounds[i] = solid_bounds(dir, content.items[i].texture);
    }
}

void assets_unload(void)
{
    for (int t = 0; t < MAX_TILES; t++) {
        if (tile_top_textures[t].id) UnloadTexture(tile_top_textures[t]);
        tile_top_textures[t] = (Texture2D){ 0 };
        if (tile_base_textures[t].id) UnloadTexture(tile_base_textures[t]);
        tile_base_textures[t] = (Texture2D){ 0 };
        if (tile_joined_textures[t].id) UnloadTexture(tile_joined_textures[t]);
        tile_joined_textures[t] = (Texture2D){ 0 };
        for (int i = 0; i < MAX_TILE_TEXTURES; i++) {
            if (tile_textures[t][i].id) UnloadTexture(tile_textures[t][i]);
            tile_textures[t][i] = (Texture2D){ 0 };
            if (tile_lit_textures[t][i].id) UnloadTexture(tile_lit_textures[t][i]);
            tile_lit_textures[t][i] = (Texture2D){ 0 };
            for (int k = 0; k < MAX_SEASONS; k++) {
                if (tile_season_textures[t][k][i].id) UnloadTexture(tile_season_textures[t][k][i]);
                tile_season_textures[t][k][i] = (Texture2D){ 0 };
            }
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

static Texture2D image_texture(const char *file);

/* Stands in for a texture that could not be loaded. */
static void draw_placeholder(Rectangle r)
{
    DrawRectangleRec(r, MAGENTA);
    DrawRectangleRec((Rectangle){ r.x, r.y, r.width / 2, r.height / 2 }, BLACK);
    DrawRectangleRec((Rectangle){ r.x + r.width / 2, r.y + r.height / 2, r.width / 2, r.height / 2 },
                     BLACK);
}

void assets_set_season(int season)
{
    season_now = season < 0 || season >= MAX_SEASONS ? 0 : season;
}

TilePart assets_tile_part(TileType t, TileType above, TileType below)
{
    if (above != t) return TILE_PART_TOP;
    return below != t ? TILE_PART_BASE : TILE_PART_BODY;
}

void assets_draw_tile(TileType t, int x, int y, float px, float py, float tile_size, float dim, TilePart part)
{
    assets_draw_tile_lit(t, x, y, px, py, tile_size, dim, part, false);
}

void assets_draw_tile_joined(TileType t, int x, int y, float px, float py, float tile_size, float dim, int joined)
{
    if (t <= TILE_AIR || t >= content.tile_count) return;
    Texture2D sheet = tile_joined_textures[t];
    if (!sheet.id) {
        assets_draw_tile(t, x, y, px, py, tile_size, dim, TILE_PART_BODY);
        return;
    }
    /* Sixteen pictures, four to a row. */
    float cell = (float)sheet.width / 4.0f;
    unsigned char v = (unsigned char)(255.0f * (dim > 1.0f ? 0.0f : 1.0f - dim));
    DrawTexturePro(sheet, (Rectangle){ (float)(joined & 3) * cell, (float)((joined >> 2) & 3) * cell, cell, cell },
                   (Rectangle){ px, py, tile_size, tile_size }, (Vector2){ 0, 0 }, 0.0f, (Color){ v, v, v, 255 });
}

void assets_draw_tile_lit(TileType t, int x, int y, float px, float py, float tile_size, float dim, TilePart part,
                          bool lit)
{
    if (t <= TILE_AIR || t >= content.tile_count) return;
    const TileDef *def = &content.tiles[t];
    /* Cheap per-position hash: which texture, and how bright. */
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    Texture2D tex = def->texture_count > 0 ? tile_textures[t][(h >> 4) % (unsigned)def->texture_count]
                                           : (Texture2D){ 0 };
    /* One that changes with the seasons wears this season's look, if it has one for it. */
    if (def->season_texture_count[season_now] > 0) {
        Texture2D worn = tile_season_textures[t][season_now][(h >> 4) % (unsigned)def->season_texture_count[season_now]];
        if (worn.id) tex = worn;
    }
    if (lit && def->lit_count > 0) {
        /* Fires do not all flicker in step: each starts somewhere of its own. */
        int frame = (int)(GetTime() * def->lit_fps + (double)(h % 7u)) % def->lit_count;
        if (tile_lit_textures[t][frame].id) tex = tile_lit_textures[t][frame];
    }
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

void assets_draw_tile_mounted(TileType t, int x, int y, float px, float py, float tile_size, float dim, int mount)
{
    if (t <= TILE_AIR || t >= content.tile_count) return;
    const TileDef *def = &content.tiles[t];
    int which = mount == MOUNT_FLOOR ? 0 : mount == MOUNT_CEILING ? 1 : mount == MOUNT_LEFT || mount == MOUNT_RIGHT ? 2 : -1;
    Texture2D tex = which >= 0 && def->texture_mount[which][0] ? image_texture(def->texture_mount[which]) : (Texture2D){ 0 };
    if (!tex.id) {
        assets_draw_tile(t, x, y, px, py, tile_size, dim, TILE_PART_BODY);
        return;
    }
    float light = 1.0f - dim;
    if (def->pulse > 0.0f) light *= 1.0f - def->pulse * (0.5f + 0.5f * sinf((float)GetTime() * 2.5f));
    unsigned char v = (unsigned char)(255.0f * (light < 0.0f ? 0.0f : light));
    DrawTexturePro(tex, (Rectangle){ 0, 0, mount == MOUNT_RIGHT ? -(float)tex.width : (float)tex.width, (float)tex.height },
                   (Rectangle){ px, py, tile_size, tile_size }, (Vector2){ 0, 0 }, 0.0f, (Color){ v, v, v, 255 });
}

void assets_draw_tile_facing(TileType t, int x, int y, float px, float py, float tile_size, float dim, bool flip)
{
    if (t <= TILE_AIR || t >= content.tile_count) return;
    const TileDef *def = &content.tiles[t];
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    Texture2D tex = def->texture_count > 0 ? tile_textures[t][(h >> 4) % (unsigned)def->texture_count] : (Texture2D){ 0 };
    if (!tex.id) {
        draw_placeholder((Rectangle){ px, py, tile_size, tile_size });
        return;
    }
    unsigned char v = (unsigned char)(255.0f * (dim > 1.0f ? 0.0f : 1.0f - dim));
    DrawTexturePro(tex, (Rectangle){ 0, 0, flip ? -(float)tex.width : (float)tex.width, (float)tex.height },
                   (Rectangle){ px, py, tile_size, tile_size }, (Vector2){ 0, 0 }, 0.0f, (Color){ v, v, v, 255 });
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
    /* Only the part of the image that is drawn on, centred. Small drawings are
     * not blown up to fill the space, and none reaches the edge. */
    Rectangle src = item_bounds[id];
    float longest = src.width > src.height ? src.width : src.height;
    float least = 0.72f * (float)(tex.width > tex.height ? tex.width : tex.height);
    float scale = r.width * ITEM_FILL / (longest > least ? longest : least);
    float w = src.width * scale, hgt = src.height * scale;
    DrawTexturePro(tex, src, (Rectangle){ r.x + (r.width - w) / 2, r.y + (r.height - hgt) / 2, w, hgt },
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
}

/* Any image under the textures directory, loaded the first time it is asked for. */
static Texture2D image_texture(const char *file)
{
    int found = -1;
    for (int i = 0; i < image_count; i++)
        if (strcmp(images[i].file, file) == 0) found = i;
    if (found < 0 && image_count < MAX_IMAGES) {
        found = image_count++;
        snprintf(images[found].file, sizeof images[found].file, "%s", file);
        images[found].texture = load(data_dir, file);
    }
    return found < 0 ? (Texture2D){ 0 } : images[found].texture;
}

static Color tone_color(int color, float light)
{
    unsigned char c[3];
    tone_rgb(color, c);
    if (light < 0.0f) light = 0.0f;
    return (Color){ (unsigned char)(c[0] * light), (unsigned char)(c[1] * light), (unsigned char)(c[2] * light), 255 };
}

void assets_draw_item_tones(ItemId id, Rectangle r, int tones)
{
    assets_draw_item(id, r);
    if (id <= ITEM_NONE || id >= content.item_count || !content.items[id].two_tone || !item_textures[id].id) return;
    const ItemDef *def = &content.items[id];
    int part[2];
    tone_unpack(tones > 0 ? tones : def->meter_start, &part[0], &part[1]);
    /* Laid exactly over the item: the same piece of the picture, in the same place. */
    Texture2D tex = item_textures[id];
    Rectangle src = item_bounds[id];
    float longest = src.width > src.height ? src.width : src.height;
    float least = 0.72f * (float)(tex.width > tex.height ? tex.width : tex.height);
    float scale = r.width * ITEM_FILL / (longest > least ? longest : least);
    float w = src.width * scale, hgt = src.height * scale;
    for (int k = 0; k < 2; k++) {
        Texture2D over = def->tone_textures[k][0] ? image_texture(def->tone_textures[k]) : (Texture2D){ 0 };
        if (!over.id) continue;
        DrawTexturePro(over, src, (Rectangle){ r.x + (r.width - w) / 2, r.y + (r.height - hgt) / 2, w, hgt },
                       (Vector2){ 0, 0 }, 0.0f, tone_color(part[k], 1.0f));
    }
}

void assets_draw_tile_tones(TileType t, float px, float py, float tile_size, float dim, int tones)
{
    if (t <= TILE_AIR || t >= content.tile_count || !content.tiles[t].two_tone) return;
    const TileDef *def = &content.tiles[t];
    int part[2];
    if (tones <= 0) tones = item_def(def->drop)->two_tone ? item_def(def->drop)->meter_start : tone_pack(0, 0);
    tone_unpack(tones, &part[0], &part[1]);
    for (int k = 0; k < 2; k++) {
        Texture2D over = def->tone_textures[k][0] ? image_texture(def->tone_textures[k]) : (Texture2D){ 0 };
        if (!over.id) continue;
        float scale = tile_size / TEXTURE_PIXELS_PER_TILE;
        float w = (float)over.width * scale, hgt = (float)over.height * scale;
        DrawTexturePro(over, (Rectangle){ 0, 0, (float)over.width, (float)over.height },
                       (Rectangle){ px + (tile_size - w) / 2, py + tile_size - hgt, w, hgt }, (Vector2){ 0, 0 }, 0.0f,
                       tone_color(part[k], 1.0f - dim));
    }
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
