#include "dao.h"

#include "mob.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool dao_on(void)
{
    return content.dao.realm_count > 0;
}

int dao_stages(void)
{
    int total = 0;
    for (int i = 0; i < content.dao.realm_count; i++) total += content.dao.realms[i].stages;
    return total;
}

int dao_realm_of(int stage, int *within)
{
    if (within) *within = 0;
    if (stage < 1) return -1;
    for (int i = 0; i < content.dao.realm_count; i++) {
        if (stage <= content.dao.realms[i].stages) {
            if (within) *within = stage;
            return i;
        }
        stage -= content.dao.realms[i].stages;
    }
    return content.dao.realm_count;
}

void game_dao_label(const Game *g, char *out, size_t size)
{
    static const char *numerals[21] = { "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X",
                                        "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX", "XX" };
    int within, realm = dao_realm_of(g->dao.stage, &within);
    if (realm < 0) snprintf(out, size, "Mortal");
    else if (realm >= content.dao.realm_count) snprintf(out, size, "%s", content.dao.immortal);
    else snprintf(out, size, "%s %s", content.dao.realms[realm].label, within <= 20 ? numerals[within] : "");
}

float game_dao_capacity(const Game *g)
{
    int within, realm = dao_realm_of(g->dao.stage, &within);
    if (realm < 0 || realm >= content.dao.realm_count) return 0.0f;
    return content.dao.realms[realm].qi_first * powf(content.dao.realms[realm].qi_growth, (float)(within - 1));
}

/* The formation a block of storage is the heart of, if it is one: its definition, or NULL. */
static const TileDef *heart_of(const Game *g, const Storage *st)
{
    if (!st->used) return NULL;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(&g->world, layer, st->x, st->y));
        if (def->formation) return def;
    }
    return NULL;
}

/* How far a lit formation reaches (kept with its heart, and worked out afresh every so often): 0 for one that is not lit. */
static int lit(const Storage *st)
{
    return st->fuel > 0 ? st->work : 0;
}

int game_formation_reach(const Game *g, int x, int y)
{
    x = world_wrap_x(&g->world, x);
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == x && g->storages[i].y == y && heart_of(g, &g->storages[i])) return lit(&g->storages[i]);
    return 0;
}

/* Whether a square is inside a lit formation. */
static bool within_formation(const Game *g, const Storage *st, int x, int y)
{
    int reach = lit(st);
    return reach > 0 && abs(world_dx(&g->world, st->x, x)) <= reach && abs(st->y - y) <= reach;
}

float game_qi_at(const Game *g, int x, int y)
{
    float qi = world_qi(&g->world, x, y), most = 0.0f;
    for (int i = 0; i < MAX_STORAGES; i++) {
        const TileDef *def = heart_of(g, &g->storages[i]);
        if (def && def->formation_qi > most && within_formation(g, &g->storages[i], x, y)) most = def->formation_qi;
    }
    return qi + most;
}

float game_dao_rate(const Game *g)
{
    const DaoDef *d = &content.dao;
    const Dao *me = &g->dao;
    if (!dao_on() || me->stage < 1 || me->stage > dao_stages() || me->manual <= ITEM_NONE || me->manual >= content.item_count) return 0.0f;
    const ItemDef *manual = item_def((ItemId)me->manual);
    float rate = d->rate * game_qi_at(g, g->player.feet.x, g->player.feet.y) * (manual->manual_rate > 0.0f ? manual->manual_rate : 1.0f);
    /* Roots: the fewer there are the purer, and one in the manual's own element is worth most of all. */
    int roots = 0;
    for (int i = 0; i < DAO_ELEMENTS; i++) roots += me->roots[i] > 0;
    rate *= roots <= 1 ? 1.5f : roots == 2 ? 1.25f : roots == 3 ? 1.0f : roots == 4 ? 0.85f : 0.7f;
    if (manual->manual_element >= 0 && manual->manual_element < DAO_ELEMENTS) rate *= 0.6f + (float)me->roots[manual->manual_element] / 100.0f;
    rate *= 1.0f + 0.05f * (float)me->meridians;
    rate *= 0.8f + 0.1f * (float)me->foundation;
    Boons have = game_boons(g);
    if (have.qi > 0.0f) rate *= have.qi;
    rate *= 1.0f - (me->toxin > 0.9f ? 0.9f : me->toxin);
    return rate;
}

float game_dao_chance(const Game *g)
{
    const DaoDef *d = &content.dao;
    int within, realm = dao_realm_of(g->dao.stage, &within);
    if (realm < 0 || realm >= d->realm_count) return 0.0f;
    const RealmDef *r = &d->realms[realm];
    /* (Into another realm it is the tribulation that says, not chance.) */
    if (within >= r->stages) return 1.0f;
    float along = r->stages > 2 ? (float)(within - 1) / (float)(r->stages - 2) : 0.0f;
    float chance = d->chance_first + (d->chance_last - d->chance_first) * along + game_boons(g).breakthrough + 0.03f * ((float)g->dao.foundation - 2.0f);
    chance -= g->dao.toxin * 0.3f;
    return chance < 0.05f ? 0.05f : chance > 1.0f ? 1.0f : chance;
}

int game_dao_needs(const Game *g, RecipeItem *out)
{
    const DaoDef *d = &content.dao;
    int within, realm = dao_realm_of(g->dao.stage, &within), n = 0;
    if (realm < 0 || realm >= d->realm_count) return 0;
    for (int i = 0; i < d->need_count[g->dao.stage] && n < 3; i++) out[n++] = d->needs[g->dao.stage][i];
    /* Coming into the next realm takes that realm's pill. */
    if (within >= d->realms[realm].stages && realm + 1 < d->realm_count && d->realms[realm + 1].pill != ITEM_NONE)
        out[n++] = (RecipeItem){ d->realms[realm + 1].pill, 1, false };
    return n;
}

bool game_dao_ready(const Game *g)
{
    RecipeItem needs[4];
    if (!dao_on() || !game_alive(g) || g->dao.stage < 1 || g->dao.stage > dao_stages() || g->dao.bolts > 0) return false;
    if (g->dao.qi + 0.001f < game_dao_capacity(g)) return false;
    if ((int)g->dao.insight < content.dao.need_insight[g->dao.stage]) return false;
    return inventory_has_all(&g->inventory, needs, game_dao_needs(g, needs));
}

/* What the realm a character is coming into sends down on them. */
static void call_down(Game *g, int bolts, float share, bool rising)
{
    g->dao.bolts = bolts;
    g->dao.bolt_share = share;
    g->dao.bolt_wait = 60;
    g->dao.rising = rising;
}

bool game_breakthrough(Game *g)
{
    const DaoDef *d = &content.dao;
    if (!game_dao_ready(g)) return false;
    RecipeItem needs[4];
    int within, realm = dao_realm_of(g->dao.stage, &within), count = game_dao_needs(g, needs);
    float chance = game_dao_chance(g);
    inventory_remove_all(&g->inventory, needs, count);
    if ((float)(game_random(g) % 10000u) / 10000.0f >= chance) {
        /* It did not take: some of the qi is gone, and it sits badly for a while. */
        g->dao.qi *= 1.0f - d->fail_loss;
        g->dao.toxin += 0.1f;
        g->dao.failed++;
        return true;
    }
    bool last = within >= d->realms[realm].stages;
    int bolts = !last ? 0 : realm + 1 < d->realm_count ? d->realms[realm + 1].bolts : d->realms[realm].bolts * 3;
    float share = !last ? 0.0f : realm + 1 < d->realm_count ? d->realms[realm + 1].bolt : d->realms[realm].bolt;
    if (bolts > 0) {
        call_down(g, bolts, share, true);
        return true;
    }
    g->dao.stage++;
    g->dao.qi = 0.0f;
    g->dao.insight += 2;
    return true;
}

bool dao_item(const ItemDef *def)
{
    return def->qi_gain > 0.0f || def->toxin != 0.0f || def->insight_gain > 0 || def->opens_meridian || def->washes_roots || def->calls_tribulation || def->manual;
}

bool game_dao_can_take(const Game *g, ItemId item)
{
    const ItemDef *def = item_def(item);
    if (!dao_on()) return false;
    /* (A manual, or anything else, that is beyond them is beyond them.) */
    if (def->needs_stage > 0 && g->dao.stage < def->needs_stage) return false;
    if (def->manual) return g->dao.manual != item;
    if (g->dao.stage < 1) return false;
    if (def->opens_meridian) return g->dao.meridians < DAO_MERIDIANS && (def->meridian == 0 || def->meridian == g->dao.meridians + 1);
    if (def->calls_tribulation) return g->dao.bolts == 0 && content.dao.called_bolts > 0;
    return true;
}

/* Gives a character the roots they were born with: how many is chance, and which, and how strong. */
static void find_roots(Game *g)
{
    static const int odds[DAO_ELEMENTS] = { 5, 15, 30, 30, 20 }; /* one root, two, three, four, all five */
    int roll = (int)(game_random(g) % 100u), many = 1;
    for (int i = 0; i < DAO_ELEMENTS; i++) {
        if (roll < odds[i]) {
            many = i + 1;
            break;
        }
        roll -= odds[i];
    }
    memset(g->dao.roots, 0, sizeof g->dao.roots);
    for (int placed = 0; placed < many; ) {
        int at = (int)(game_random(g) % (uint32_t)DAO_ELEMENTS);
        if (g->dao.roots[at] > 0) continue;
        g->dao.roots[at] = (uint8_t)(40 + game_random(g) % 61u);
        placed++;
    }
}

void dao_take(Game *g, ItemId item)
{
    const ItemDef *def = item_def(item);
    Dao *me = &g->dao;
    if (def->manual) {
        /* The first one studied is the beginning of the road. */
        if (me->stage < 1) {
            me->stage = 1;
            me->qi = 0.0f;
            me->foundation = 2;
            find_roots(g);
        }
        me->manual = (uint16_t)item;
    }
    if (def->washes_roots) find_roots(g);
    if (def->opens_meridian && me->meridians < DAO_MERIDIANS) me->meridians++;
    if (def->qi_gain > 0.0f && me->stage >= 1) {
        float room = game_dao_capacity(g);
        me->qi += def->qi_gain;
        if (me->qi > room) me->qi = room;
    }
    me->toxin += def->toxin;
    me->insight += (uint32_t)def->insight_gain;
    if (me->toxin > 1.0f) me->toxin = 1.0f;
    if (me->toxin < 0.0f) me->toxin = 0.0f;
    if (def->calls_tribulation) call_down(g, content.dao.called_bolts, content.dao.called_bolt, false);
}

/* What they have on counts only if they have got far enough for it. */
static bool fit_for(const Game *g, const ItemDef *def)
{
    return def->needs_stage <= 0 || g->dao.stage >= def->needs_stage;
}

void dao_boons(const Game *g, Boons *all)
{
    const DaoDef *d = &content.dao;
    if (!dao_on() || g->dao.stage < 1) return;
    int stage = g->dao.stage > dao_stages() ? dao_stages() : g->dao.stage, realm = dao_realm_of(g->dao.stage, NULL);
    float gain = 1.0f + d->stage_gain * (float)stage * (0.8f + 0.1f * (float)g->dao.foundation);
    all->walk *= 1.0f + (gain - 1.0f) * 0.5f;
    all->mine *= gain;
    all->craft *= gain;
    all->regen *= gain;
    all->strength += stage / 4;
    all->armor += 0.004f * (float)stage;
    Boons step;
    for (int i = 0; i <= realm && i < d->realm_count; i++) {
        step = d->realms[i].boons;
        if (step.walk > 0.0f) all->walk *= step.walk;
        if (step.mine > 0.0f) all->mine *= step.mine;
        if (step.craft > 0.0f) all->craft *= step.craft;
        if (step.regen > 0.0f) all->regen *= step.regen;
        if (step.lung > 0.0f) all->lung *= step.lung;
        if (step.appetite > 0.0f) all->appetite *= step.appetite;
        if (step.stamina > 0.0f) all->stamina *= step.stamina;
        all->armor += step.armor, all->warmth += step.warmth, all->luck += step.luck, all->strength += step.strength, all->thorns += step.thorns;
        all->ward += step.ward, all->breakthrough += step.breakthrough;
        if (step.glide > all->glide) all->glide = step.glide;
        if (step.sight > all->sight) all->sight = step.sight;
        if (step.unseen > all->unseen) all->unseen = step.unseen;
        if (step.magnet > all->magnet) all->magnet = step.magnet;
        if (step.fireproof > all->fireproof) all->fireproof = step.fireproof;
        if (step.flight > all->flight) all->flight = step.flight;
    }
    if (realm >= d->realm_count) {
        step = d->immortal_boons;
        if (step.walk > 0.0f) all->walk *= step.walk;
        if (step.regen > 0.0f) all->regen *= step.regen;
        if (step.lung > 0.0f) all->lung *= step.lung;
        all->armor += step.armor, all->luck += step.luck, all->strength += step.strength, all->ward += step.ward;
        if (step.fireproof > all->fireproof) all->fireproof = step.fireproof;
        if (step.flight > all->flight) all->flight = step.flight;
        if (step.sight > all->sight) all->sight = step.sight;
        if (step.magnet > all->magnet) all->magnet = step.magnet;
    }
    /* Whatever formation they are standing in. */
    for (int i = 0; i < MAX_STORAGES; i++) {
        const TileDef *def = heart_of(g, &g->storages[i]);
        if (!def || !within_formation(g, &g->storages[i], g->player.feet.x, g->player.feet.y)) continue;
        if (def->aura.regen > 0.0f) all->regen *= def->aura.regen;
        if (def->aura.craft > 0.0f) all->craft *= def->aura.craft;
        if (def->aura.walk > 0.0f) all->walk *= def->aura.walk;
        if (def->aura.qi > 0.0f) all->qi = (all->qi > 0.0f ? all->qi : 1.0f) * def->aura.qi;
        all->armor += def->aura.armor, all->ward += def->aura.ward, all->breakthrough += def->aura.breakthrough, all->warmth += def->aura.warmth;
        if (def->aura.unseen > all->unseen) all->unseen = def->aura.unseen;
        if (def->aura.fireproof > all->fireproof) all->fireproof = def->aura.fireproof;
        if (def->aura.sight > all->sight) all->sight = def->aura.sight;
    }
    (void)fit_for;
}

void dao_tick(Game *g)
{
    const DaoDef *d = &content.dao;
    Dao *me = &g->dao;
    if (!dao_on() || me->stage < 1) return;
    if (me->toxin > 0.0f) {
        me->toxin -= d->toxin_fade;
        if (me->toxin < 0.0f) me->toxin = 0.0f;
    }
    /* Sitting, they gather it: up to what the stage holds, and no more till they have broken through. */
    if (g->vitals.rest == REST_MEDITATE && game_alive(g) && me->stage <= dao_stages()) {
        float room = game_dao_capacity(g);
        me->qi += game_dao_rate(g);
        if (me->qi > room) me->qi = room;
    }
    /* A tribulation: the bolts come one after another, each a share of a health bar that only wards turn. */
    if (me->bolts > 0 && game_alive(g)) {
        if (--me->bolt_wait > 0) return;
        float ward = game_boons(g).ward, share = me->bolt_share * (1.0f - (ward > 0.9f ? 0.9f : ward));
        g->lightning = 8;
        g->lightning_x = g->player.feet.x;
        game_wake(g);
        game_hurt(g, share);
        me->bolts--;
        me->bolt_wait = 40;
        if (!game_alive(g)) {
            /* It was too much for them: what they had gathered is gone, and what they had built is the worse for it. */
            me->bolts = 0;
            if (me->rising) {
                me->qi = 0.0f;
                if (me->foundation > 0) me->foundation--;
            }
            return;
        }
        if (me->bolts > 0) return;
        if (me->rising) {
            /* Through it. How well they came through is how good a foundation the new realm is built on. */
            int quality = 1 + (g->vitals.health > 0.3f) + (g->vitals.health > 0.6f) + (me->toxin < 0.2f);
            me->foundation = (uint8_t)(quality > 4 ? 4 : quality);
            me->stage++;
            me->qi = 0.0f;
        } else if (d->called_leaves.id != ITEM_NONE) {
            game_give_items(g, d->called_leaves.id, d->called_leaves.count);
        }
        me->insight += 25;
        /* Where it struck, something grows that grows nowhere else. */
        for (int dx = -3; d->scorch != TILE_AIR && dx <= 3; dx++) {
            int sx = g->player.feet.x + dx, sy = g->player.feet.y, layer = tile_def(d->scorch)->layer;
            if (game_random(g) % 2u != 0 || !world_is_solid(&g->world, sx, sy + 1) || world_is_solid(&g->world, sx, sy) || world_get_at(&g->world, layer, sx, sy) != TILE_AIR) continue;
            world_set_at(&g->world, layer, sx, sy, d->scorch);
        }
    }
}

void dao_world_tick(Game *g)
{
    World *w = &g->world;
    if (g->lightning > 0) g->lightning--;
    if (g->tick % 10 != 0) return;
    static const int cross[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }, square[4][2] = { { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        const TileDef *def = heart_of(g, st);
        if (!def) continue;
        /* Lit if its flags stand in its pattern, every one the same way off: the furthest such there is. */
        int reach = 0;
        for (int far = def->formation_max; far >= def->formation_min && reach == 0; far--) {
            bool all = true;
            for (int k = 0; k < 4 && all && def->formation_pattern != 1; k++)
                all = world_get_at(w, tile_def(def->formation_flags)->layer, st->x + cross[k][0] * far, st->y + cross[k][1] * far) == def->formation_flags;
            for (int k = 0; k < 4 && all && def->formation_pattern != 0; k++)
                all = world_get_at(w, tile_def(def->formation_flags)->layer, st->x + square[k][0] * far, st->y + square[k][1] * far) == def->formation_flags;
            if (all) reach = far;
        }
        st->work = (int16_t)reach;
        if (lit(st) == 0) continue;
        /* What grows inside it is hurried; what does not belong inside it is hurt. */
        for (int n = 0; n < (int)(def->formation_grows * 2.0f + 0.5f); n++) {
            int x = st->x + (int)(game_random(g) % (uint32_t)(2 * reach + 1)) - reach, y = st->y + (int)(game_random(g) % (uint32_t)(2 * reach + 1)) - reach;
            for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
                int slow = tile_def(world_get_at(w, layer, x, y))->grow_one_in;
                if (slow > 1 && game_random(g) % (uint32_t)slow != 0) continue;
                world_grow_plant(w, layer, x, y);
            }
        }
        for (int m = 0; def->formation_harms > 0.0f && g->tick % 20 == 0 && m < MAX_MOBS; m++)
            if (g->mobs[m].active && !g->mobs[m].owned && within_formation(g, st, g->mobs[m].x, g->mobs[m].y)) mob_hurt(g, m, (int)def->formation_harms);
    }
}

bool game_technique_ready(const Game *g, ItemId item)
{
    const ItemDef *def = item_def(item);
    if (!def->technique) return true;
    return dao_on() && g->dao.stage >= def->technique_stage && g->dao.qi >= def->technique_qi && (int)g->dao.insight >= def->technique_insight;
}

void dao_spend(Game *g, ItemId item)
{
    const ItemDef *def = item_def(item);
    if (!def->technique) return;
    g->dao.qi -= def->technique_qi;
    if (g->dao.qi < 0.0f) g->dao.qi = 0.0f;
    /* Using what they know teaches them something - up to a point, for the stage they are at. */
    if ((int)g->dao.insight < g->dao.stage * 10) g->dao.insight++;
}

bool dao_revive(Game *g)
{
    const DaoDef *d = &content.dao;
    int realm = dao_realm_of(g->dao.stage, NULL);
    bool can = false;
    for (int i = 0; i <= realm && i < d->realm_count; i++) can = can || d->realms[i].revives;
    uint64_t day = (uint64_t)(content.rules.day_ticks + content.rules.night_ticks);
    if (!dao_on() || !can || g->dao.bolts > 0 || (g->dao.revived != 0 && g->tick - g->dao.revived < (day > 0 ? day : 1))) return false;
    g->dao.revived = g->tick > 0 ? g->tick : 1;
    g->vitals.health = 0.5f;
    g->vitals.death = 1.0f;
    g->vitals.poison = 0;
    game_cancel_actions(g);
    g->player.feet = g->player.step_to = (TilePos){ g->world.spawn_x, g->world.spawn_y };
    g->player.activity = ACTIVITY_IDLE;
    g->player.climbing = false;
    g->travels++;
    return true;
}

bool game_formation_gate(Game *g, int x, int y)
{
    const World *w = &g->world;
    const Storage *from = NULL, *to = NULL;
    x = world_wrap_x(w, x);
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == x && g->storages[i].y == y) from = &g->storages[i];
    const TileDef *def = from ? heart_of(g, from) : NULL;
    if (!def || !def->formation_gate || lit(from) == 0) return false;
    long best = -1;
    for (int i = 0; i < MAX_STORAGES; i++) {
        const Storage *st = &g->storages[i];
        const TileDef *other = heart_of(g, st);
        if (st == from || !other || !other->formation_gate || lit(st) == 0) continue;
        long dx = world_dx(w, x, st->x), dy = st->y - y, far = dx * dx + dy * dy;
        if (best < 0 || far < best) best = far, to = st;
    }
    if (!to) return false;
    game_cancel_actions(g);
    game_close_bench(g);
    g->player.feet = g->player.step_to = (TilePos){ to->x, to->y };
    g->player.activity = ACTIVITY_IDLE;
    g->player.climbing = false;
    g->travels++;
    game_reveal(g, to->x, to->y - 1);
    return true;
}
