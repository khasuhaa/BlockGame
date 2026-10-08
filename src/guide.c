#include "guide.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Adds to the end of a sentence being put together. */
static void say(char *out, size_t size, const char *format, ...)
{
    size_t len = strlen(out);
    if (len + 1 >= size) return;
    va_list args;
    va_start(args, format);
    vsnprintf(out + len, size - len, format, args);
    va_end(args);
}

static const char *tile_label(TileType t)
{
    const TileDef *d = tile_def(t);
    return d->label[0] ? d->label : d->name;
}

/* A name from the data files made fit to read: "cherry_grove" as "cherry grove". */
static const char *readable(const char *name)
{
    static char buffers[4][CONTENT_NAME_LEN];
    static int next;
    char *out = buffers[next++ % 4];
    snprintf(out, CONTENT_NAME_LEN, "%s", name);
    for (char *c = out; *c; c++)
        if (*c == '_') *c = ' ';
    return out;
}

/* "a" or "an", as the word after it wants. */
static const char *a(const char *word)
{
    return strchr("AEIOUaeiou", word[0]) && word[0] ? "an" : "a";
}

static const char *biome_label(int b)
{
    return content.biomes[b].label[0] ? content.biomes[b].label : readable(content.biomes[b].name);
}

/* Whether block `t` is `first` or something `first` grows into in time:
 * the top of a plant that ends up two blocks tall included. */
static bool grows_from(TileType first, TileType t)
{
    for (int steps = 0; first != TILE_AIR && steps < MAX_TILES; steps++) {
        if (first == t || (tile_def(first)->grows_top == t && t != TILE_AIR)) return true;
        first = tile_def(first)->grows_into;
    }
    return false;
}

/* The biomes a tree grows in, as a list; false if there are none. (One that
 * grows on the islands in the sky is said to, with how high up.) */
static bool tree_biomes(int tree, char *out, size_t size)
{
    int found = 0;
    for (int i = 0; i < content.worldgen.island_tree_count && content.worldgen.island_rate > 0.0f; i++) {
        if (content.worldgen.island_trees[i].tree != tree) continue;
        say(out, size, "%sislands in the sky, above Y %d", found++ ? ", " : "", content.worldgen.island_trees[i].min_y);
    }
    for (int b = 0; b < content.biome_count; b++) {
        for (int i = 0; i < content.biomes[b].tree_count; i++) {
            if (content.biomes[b].trees[i].tree != tree) continue;
            say(out, size, "%s%s", found++ ? ", " : "", biome_label(b));
            break;
        }
    }
    return found > 0;
}

/* Whether a creature is to be met in a new world: it turns up by itself, or
 * is bred from one that does. */
static bool mob_natural(int mob, int depth)
{
    if (content.mobs[mob].spawn_chance > 0.0f) return true;
    if (depth > content.mob_count) return false;
    for (int m = 0; m < content.mob_count; m++)
        for (int i = 0; i < content.mobs[m].lay_next_count; i++)
            if (content.mobs[m].lay_next[i] == mob && m != mob && mob_natural(m, depth + 1)) return true;
    return false;
}

static const char *mob_label(int mob)
{
    return content.mobs[mob].label[0] ? content.mobs[mob].label : readable(content.mobs[mob].name);
}

/* Where a creature comes from. */
static void mob_where(int mob, char *out, size_t size)
{
    const MobDef *def = &content.mobs[mob];
    if (def->spawn_chance > 0.0f) {
        if (def->perches) say(out, size, "Lives in the tops of trees.");
        else if (def->spawn_water && def->spawn_biome_count > 0) say(out, size, "Swims in the %s.", biome_label(def->spawn_biomes[0]));
        else if (def->spawn_water) say(out, size, "Swims in open water.");
        else {
            say(out, size, "Turns up in the wild");
            for (int i = 0; i < def->spawn_above_count; i++)
                say(out, size, "%s%s", i == 0 ? ", on " : " or ", tile_label(def->spawn_above[i]));
            for (int i = 0; i < def->spawn_biome_count; i++)
                say(out, size, "%s%s", i == 0 ? ", in: " : ", ", biome_label(def->spawn_biomes[i]));
            say(out, size, ".");
        }
        return;
    }
    for (int m = 0; m < content.mob_count; m++) {
        for (int i = 0; i < content.mobs[m].lay_next_count; i++) {
            if (content.mobs[m].lay_next[i] != mob || m == mob) continue;
            say(out, size, "Hatches from some of the eggs %s %s lays (%d%% of them, shared among its kinds).", a(mob_label(m)), mob_label(m),
                (int)(content.mobs[m].lay_next_chance * 100.0f + 0.5f));
            return;
        }
    }
}

static bool natural(TileType t, char *out, size_t size, int depth)
{
    const WorldGenDef *wg = &content.worldgen;
    if (t <= TILE_AIR || t >= content.tile_count || depth > 8) return false;
    out[0] = '\0';

    for (int i = 0; i < content.vein_count; i++) {
        const VeinDef *v = &content.veins[i];
        if (v->tile != t) continue;
        if (v->shape == VEIN_SCAR) say(out, size, "In great scars, %d to %d blocks long", v->min_size, v->max_size);
        else say(out, size, "In veins of %d to %d", v->min_size, v->max_size);
        for (int h = 0; h < v->host_count; h++) say(out, size, "%s%s", h == 0 ? " in " : " or ", tile_label(v->hosts[h]));
        say(out, size, ", %d to %d blocks down.", v->min_depth, v->max_depth);
        return true;
    }
    for (int i = 0; i < content.decor_count; i++) {
        const DecorDef *d = &content.decor[i];
        if (d->tile != t) continue;
        if (d->hang_max > 0) say(out, size, "Hanging from the roofs of caves, %d to %d blocks down.", d->min_depth, d->max_depth);
        else say(out, size, "On the %s of caves, %d to %d blocks down.", d->on_floor ? "floors" : "walls", d->min_depth, d->max_depth);
        return true;
    }
    for (int i = 0; i < wg->pole_count; i++) {
        if (wg->poles[i].tile == t) return say(out, size, "The %s stands of it.", wg->poles[i].label), true;
        if (wg->poles[i].top == t) return say(out, size, "On top of the %s.", wg->poles[i].label), true;
    }
    if (t == wg->rock) return say(out, size, "The rock under everything."), true;
    {
        int found = 0;
        bool top = false;
        for (int b = 0; b < content.biome_count; b++) {
            const BiomeDef *bio = &content.biomes[b];
            if (bio->surface != t && bio->soil != t) continue;
            if (found == 0) top = bio->surface == t;
            say(out, size, "%s%s", found++ ? ", " : top ? "The ground of: " : "Under the ground of: ", biome_label(b));
        }
        if (found) return say(out, size, "."), true;
        if (t == wg->surface) return say(out, size, "The ground of the plains."), true;
        if (t == wg->soil) return say(out, size, "Under the ground everywhere."), true;
    }
    for (int b = 0, found = 0; b <= content.biome_count; b++) {
        if (b == content.biome_count) {
            if (found) return say(out, size, "."), true;
            break;
        }
        if (content.biomes[b].under != t || content.biomes[b].under_depth <= 0) continue;
        say(out, size, "%s%s", found++ ? ", " : "The rock under: ", biome_label(b));
    }
    for (int b = 0, found = 0; b <= content.biome_count; b++) {
        if (b == content.biome_count) {
            if (found) return say(out, size, "."), true;
            break;
        }
        if (content.biomes[b].seam != t || content.biomes[b].under == TILE_AIR) continue;
        if (!found) say(out, size, "Mixed through the lowest layer of the ground, on the %s, under: ", tile_label(content.biomes[b].under));
        say(out, size, "%s%s", found++ ? ", " : "", biome_label(b));
    }
    for (int b = 0, found = 0; b <= content.biome_count; b++) {
        if (b == content.biome_count) {
            if (found) return say(out, size, "."), true;
            break;
        }
        if (content.biomes[b].buried != t) continue;
        say(out, size, "%s%s", found++ ? ", " : "Buried under: ", biome_label(b));
    }
    for (int b = 0, found = 0; b <= content.biome_count; b++) {
        if (b == content.biome_count) {
            if (found) return say(out, size, "."), true;
            break;
        }
        for (int i = 0; i < content.biomes[b].plant_count; i++) {
            if (!grows_from(content.biomes[b].plants[i].tile, t)) continue;
            say(out, size, "%s%s", found++ ? ", " : "Grows wild in: ", biome_label(b));
            break;
        }
    }
    for (int i = 0; i < content.tree_count; i++) {
        const TreeDef *tree = &content.trees[i];
        if (tree->log != t && tree->leaves != t) continue;
        char biomes[160] = "";
        if (!tree_biomes(i, biomes, sizeof biomes)) continue;
        say(out, size, "The %s of %s trees, in: %s.", tree->log == t ? "trunk" : "leaves", readable(tree->name), biomes);
        return true;
    }
    /* What something in the world turns into. */
    for (TileType o = 1; o < content.tile_count; o++) {
        const TileDef *od = &content.tiles[o];
        char from[200];
        if (o == t) continue;
        if (od->decays_into == t && natural(o, from, sizeof from, depth + 1)) {
            say(out, size, "What %s is left as when the tree is cut through beneath it.", tile_label(o));
            return true;
        }
    }
    for (int m = 0; m < content.mob_count; m++) {
        if (content.mobs[m].lays != t || !mob_natural(m, 0)) continue;
        say(out, size, "Laid by %s %s.", a(mob_label(m)), mob_label(m));
        return true;
    }
    if (t == wg->portal_base || t == wg->portal_body || t == wg->portal_top) return say(out, size, "Stands where a world begins."), true;
    return false;
}

bool guide_tile_natural(TileType t, char *where, size_t size)
{
    char scratch[200];
    return where ? natural(t, where, size, 0) : natural(t, scratch, sizeof scratch, 0);
}

ItemId guide_placer(TileType t)
{
    for (ItemId i = 1; i < content.item_count; i++) {
        const ItemDef *it = &content.items[i];
        if (it->places == t && it->places != TILE_AIR) return i;
        for (int s = 0; s < MAX_ITEM_SIZES; s++)
            if (it->sized && it->places_sized[s] == t && t != TILE_AIR) return i;
    }
    /* Not put down as it is: grown, then, from something sown or planted. */
    for (ItemId i = 1; i < content.item_count; i++) {
        const ItemDef *it = &content.items[i];
        if (it->sows != TILE_AIR && grows_from(it->sows, t)) return i;
        if (it->plants >= 0 && it->plants < content.tree_count &&
            (content.trees[it->plants].log == t || content.trees[it->plants].leaves == t))
            return i;
    }
    return ITEM_NONE;
}

TileType guide_bench_tile(const char *kind, int level)
{
    TileType best = TILE_AIR;
    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *d = &content.tiles[t];
        if (!d->bench || strcmp(d->bench_kind, kind) != 0) continue;
        if (d->bench_level == level) return t;
        if (best == TILE_AIR || d->bench_level < tile_def(best)->bench_level) best = t;
    }
    return best;
}

static ItemSource *add(ItemSource *out, int *count, int max, SourceKind kind, int from, int min, int most, float chance)
{
    static ItemSource spare;
    ItemSource *s = *count < max ? &out[(*count)++] : &spare;
    memset(s, 0, sizeof *s);
    s->kind = kind;
    s->from = from;
    s->min = min;
    s->max = most;
    s->chance = chance;
    s->tool_class = -1;
    s->placed_by = ITEM_NONE;
    return s;
}

/* Fills in where a block is found: in the world, or because something puts
 * it there. False if neither - or if the only way to it is the item itself. */
static bool place(ItemSource *s, TileType t, ItemId item)
{
    const TileDef *def = tile_def(t);
    snprintf(s->label, sizeof s->label, "%s", tile_label(t));
    s->tool_class = def->tool_class;
    if (natural(t, s->where, sizeof s->where, 0)) return true;
    /* A bench raised from the one below it. */
    for (TileType o = 1; o < content.tile_count; o++) {
        const TileDef *od = &content.tiles[o];
        if (od->upgrade_to != t || o == t) continue;
        say(s->where, sizeof s->where, "%s raised a tier", tile_label(o));
        for (int c = 0; c < od->upgrade_cost_count; c++)
            say(s->where, sizeof s->where, "%s%d %s", c == 0 ? ", for " : ", ", od->upgrade_cost[c].count,
                item_def(od->upgrade_cost[c].id)->label);
        say(s->where, sizeof s->where, ", and taken up.");
        s->placed_by = guide_placer(o);
        return true;
    }
    s->placed_by = guide_placer(t);
    if (s->placed_by == ITEM_NONE) return false;
    const ItemDef *by = item_def(s->placed_by);
    bool put_down = by->places == t;
    for (int k = 0; k < MAX_ITEM_SIZES; k++) put_down = put_down || (by->sized && by->places_sized[k] == t);
    if (put_down) {
        if (s->placed_by == item) return false; /* putting a thing down and taking it up again is no way to get one */
        say(s->where, sizeof s->where, "%s that has been put down.", by->label);
    } else if (by->sows != TILE_AIR) {
        say(s->where, sizeof s->where, "Grown from %s", by->label);
        for (int i = 0; i < tile_def(by->sows)->soil_count; i++)
            say(s->where, sizeof s->where, "%s%s", i == 0 ? ", sown on " : " or ", tile_label(tile_def(by->sows)->soil[i]));
        say(s->where, sizeof s->where, "%s.", tile_def(by->sows)->needs_water ? ", under water" : "");
    } else {
        say(s->where, sizeof s->where, "Part of the tree %s %s grows into.", a(by->label), by->label);
    }
    return true;
}

int guide_sources(ItemId item, ItemSource *out, int max)
{
    int count = 0;
    if (item <= ITEM_NONE || item >= content.item_count) return 0;

    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *def = &content.tiles[t];
        int many = 0;
        float sometimes = 1.0f;
        if (def->drop == item) many = def->drop_count > 0 ? def->drop_count : 1;
        for (int i = 0; i < def->extra_drop_count; i++) {
            if (def->extra_drops[i].id != item) continue;
            many = def->extra_drops[i].count;
            sometimes = def->extra_chance[i];
        }
        if (many > 0 && (def->removable || def->vanishes)) {
            int before = count;
            ItemSource *s = add(out, &count, max, SOURCE_BLOCK, t, many, many,
                                (def->drop_chance > 0.0f ? def->drop_chance : 1.0f) * sometimes);
            bool keep = place(s, t, item);
            /* The stages of a plant, or the pictures of one block, are one line. */
            for (int i = 0; keep && i < before; i++)
                if (out[i].kind == SOURCE_BLOCK && strcmp(out[i].label, s->label) == 0 && out[i].min == s->min &&
                    out[i].chance == s->chance)
                    keep = false;
            if (!keep) count = before;
        }
        for (int i = 0; i < def->loot_count; i++) {
            const LootDef *l = &def->loot[i];
            int weight = 0, total = 0;
            for (int c = 0; c < l->choice_count; c++) {
                total += l->choices[c].weight;
                if (l->choices[c].item == item) weight += l->choices[c].weight;
            }
            /* (What is found there by the place it stands in: a line to every place that has this in its pool.) */
            for (int p = 0; l->area && p < content.pool_count; p++) {
                const PoolDef *pool = &content.pools[p];
                int has = 0, of = 0;
                for (int k = 0; k < pool->loot_count; k++) {
                    of += pool->loot[k].weight;
                    if (pool->loot[k].item == item) has += pool->loot[k].weight;
                }
                if (has == 0 || (!pool->any_biome && pool->biome_count == 0)) continue;
                ItemSource *s = add(out, &count, max, SOURCE_CHEST, t, l->min, l->max, (float)has / (float)of);
                snprintf(s->label, sizeof s->label, "%s", tile_label(t));
                snprintf(s->where, sizeof s->where, "%s", pool->label);
            }
            if (l->item != item && weight == 0) continue;
            int before = count;
            ItemSource *s = add(out, &count, max, SOURCE_CHEST, t, l->min, l->max,
                                l->item == item || total <= 0 ? 1.0f : (float)weight / (float)total);
            snprintf(s->label, sizeof s->label, "%s", tile_label(t));
            if (!natural(t, s->where, sizeof s->where, 0)) count = before; /* one the player put down holds nothing */
        }
    }
    for (int m = 0; m < content.mob_count; m++) {
        const MobDef *def = &content.mobs[m];
        for (int i = 0; i < def->drop_count; i++) {
            if (def->drops[i].item != item || !mob_natural(m, 0)) continue;
            ItemSource *s = add(out, &count, max, SOURCE_CREATURE, m, def->drops[i].min, def->drops[i].max,
                                def->drops[i].chance > 0.0f ? def->drops[i].chance : 1.0f);
            snprintf(s->label, sizeof s->label, "%s", mob_label(m));
            mob_where(m, s->where, sizeof s->where);
        }
    }
    for (int m = 0; m < content.mob_count; m++) {
        const MobDef *def = &content.mobs[m];
        if (def->shear_item != item || !mob_natural(m, 0)) continue;
        ItemSource *s = add(out, &count, max, SOURCE_SHEAR, m, def->shear_min, def->shear_max, 1.0f);
        snprintf(s->label, sizeof s->label, "%s", mob_label(m));
        for (ItemId tool = 1; tool < content.item_count && s->placed_by == ITEM_NONE; tool++)
            if (content.items[tool].shears) s->placed_by = tool;
        if (s->placed_by != ITEM_NONE)
            say(s->where, sizeof s->where, "With %s %s in hand. ", a(item_def(s->placed_by)->label), item_def(s->placed_by)->label);
        say(s->where, sizeof s->where, "It grows back in %d ticks. ", def->shear_ticks);
        mob_where(m, s->where, sizeof s->where);
    }
    for (int m = 0; m < content.mob_count; m++) {
        const MobDef *def = &content.mobs[m];
        if (def->milk_item != item || !mob_natural(m, 0)) continue;
        ItemSource *s = add(out, &count, max, SOURCE_MILK, m, 1, 1, 1.0f);
        snprintf(s->label, sizeof s->label, "%s", mob_label(m));
        for (ItemId tool = 1; tool < content.item_count && s->placed_by == ITEM_NONE; tool++)
            if (content.items[tool].milks) s->placed_by = tool;
        if (s->placed_by != ITEM_NONE)
            say(s->where, sizeof s->where, "With an empty %s in hand. ", item_def(s->placed_by)->label);
        say(s->where, sizeof s->where, "It has more in %d ticks. ", def->milk_ticks);
        mob_where(m, s->where, sizeof s->where);
    }
    for (int i = 0; i < content.tree_count; i++) {
        const TreeDef *tree = &content.trees[i];
        if (tree->fruit != item || item == ITEM_NONE) continue;
        char biomes[160] = "";
        ItemSource *s = add(out, &count, max, SOURCE_FRUIT, i, 1, 1, 1.0f);
        snprintf(s->label, sizeof s->label, "%s tree", readable(tree->name));
        if (s->label[0]) s->label[0] = (char)toupper((unsigned char)s->label[0]);
        if (tree_biomes(i, biomes, sizeof biomes)) {
            say(s->where, sizeof s->where, "Grows in: %s. The fruit hangs among the leaves.", biomes);
        } else {
            for (ItemId p = 1; p < content.item_count; p++)
                if (content.items[p].plants == i) s->placed_by = p;
            if (s->placed_by == ITEM_NONE) count--;
            else say(s->where, sizeof s->where, "Grown from %s %s.", a(item_def(s->placed_by)->label), item_def(s->placed_by)->label);
        }
    }
    {
        int total = 0, weight = 0;
        for (int i = 0; i < content.rules.loot_count; i++) {
            total += content.rules.loot[i].weight;
            if (content.rules.loot[i].item == item) weight += content.rules.loot[i].weight;
        }
        if (weight > 0) {
            ItemSource *s = add(out, &count, max, SOURCE_MEDITATION, 0, 1, 1, (float)weight / (float)total);
            snprintf(s->label, sizeof s->label, "Meditating");
            say(s->where, sizeof s->where, "Every %d ticks of meditating brings something.", content.rules.meditation_ticks);
        }
    }
    if (item_def(item)->speciality) {
        ItemSource *s = add(out, &count, max, SOURCE_VILLAGE, 0, 1, 1, 1.0f);
        snprintf(s->label, sizeof s->label, "A village");
        say(s->where, sizeof s->where, "A speciality: sold at the trade portal of some villages and no others.");
    }
    for (int i = 0; i < content.rules.start_item_count; i++) {
        if (content.rules.start_items[i].id != item) continue;
        int many = content.rules.start_items[i].count;
        ItemSource *s = add(out, &count, max, SOURCE_START, 0, many, many, 1.0f);
        snprintf(s->label, sizeof s->label, "A new character");
        say(s->where, sizeof s->where, "The first character of a world starts with it.");
    }
    return count;
}

int guide_recipes(ItemId item, int *out, int max)
{
    int count = 0;
    for (int i = 0; i < content.recipe_count && count < max; i++) {
        bool makes = content.recipes[i].output.id == item;
        /* (One that makes it some of the time counts.) */
        for (int k = 0; k < content.recipes[i].one_of_count; k++) makes = makes || content.recipes[i].one_of[k].item == item;
        if (makes && item != ITEM_NONE) out[count++] = i;
    }
    return count;
}

int guide_uses(ItemId item, int *out, int max)
{
    int count = 0;
    for (int i = 0; i < content.recipe_count && count < max; i++) {
        const Recipe *r = &content.recipes[i];
        bool uses = false;
        for (int k = 0; k < r->input_count; k++) uses = uses || r->inputs[k].id == item;
        for (int k = 0; k < r->random_item_count; k++) uses = uses || r->random_items[k] == item;
        if (uses && item != ITEM_NONE) out[count++] = i;
    }
    return count;
}

bool guide_matches(ItemId item, const char *text)
{
    const char *label = item_def(item)->label;
    size_t n = strlen(text);
    if (n == 0) return true;
    for (const char *at = label; *at; at++) {
        size_t i = 0;
        while (i < n && at[i] && tolower((unsigned char)at[i]) == tolower((unsigned char)text[i])) i++;
        if (i == n) return true;
    }
    return false;
}

static int by_listing(const void *a, const void *b)
{
    ItemId ia = *(const ItemId *)a, ib = *(const ItemId *)b;
    const ItemDef *da = item_def(ia), *db = item_def(ib);
    int ca = da->category < 0 ? content.trade.category_count : da->category;
    int cb = db->category < 0 ? content.trade.category_count : db->category;
    if (ca != cb) return ca - cb;
    for (const char *x = da->label, *y = db->label; *x || *y; x++, y++) {
        int lx = tolower((unsigned char)*x), ly = tolower((unsigned char)*y);
        if (lx != ly) return lx - ly;
        if (!*x || !*y) break;
    }
    return (int)ia - (int)ib;
}

int guide_order(ItemId *out)
{
    int n = 0;
    for (ItemId i = 1; i < content.item_count; i++) out[n++] = i;
    qsort(out, (size_t)n, sizeof out[0], by_listing);
    return n;
}
