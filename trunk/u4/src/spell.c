/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "u4.h"
#include "spell.h"
#include "direction.h"
#include "context.h"
#include "map.h"
#include "moongate.h"
#include "annotation.h"
#include "ttype.h"
#include "screen.h"
#include "player.h"
#include "debug.h"

SpellCallback spellCallback = NULL; 

static int spellAwaken(int player);
static int spellBlink(int dir);
static int spellCure(int player);
static int spellDispel(int dir);
static int spellEField(int dir);
static int spellFireball(int dir);
static int spellGate(int phase);
static int spellHeal(int player);
static int spellIceball(int dir);
static int spellJinx(int unused);
static int spellKill(int dir);
static int spellLight(int unused);
static int spellMMissle(int dir);
static int spellNegate(int unused);
static int spellOpen(int unused);
static int spellProtect(int unused);
static int spellRez(int player);
static int spellQuick(int unused);
static int spellSleep(int unused);
static int spellTremor(int unused);
static int spellUndead(int unused);
static int spellView(int unsued);
static int spellWinds(int fromdir);
static int spellXit(int unused);
static int spellYup(int unused);
static int spellZdown(int unused);

/* masks for reagents */
#define ASH (1 << REAG_ASH)
#define GINSENG (1 << REAG_GINSENG)
#define GARLIC (1 << REAG_GARLIC)
#define SILK (1 << REAG_SILK)
#define MOSS (1 << REAG_MOSS)
#define PEARL (1 << REAG_PEARL)
#define NIGHTSHADE (1 << REAG_NIGHTSHADE)
#define MANDRAKE (1 << REAG_MANDRAKE)

/* spell context flags */
#define SC_NORMAL  (1 << 0)
#define SC_DUNGEON (1 << 1)
#define SC_COMBAT  (1 << 2)
#define SC_ANY (SC_NORMAL | SC_DUNGEON | SC_COMBAT)

const Spell spells[] = {
    { "Awaken",       GINSENG | GARLIC,         SC_ANY,      &spellAwaken,  SPELLPRM_PLAYER,  5 },
    { "Blink",        SILK | MOSS,              SC_NORMAL,   &spellBlink,   SPELLPRM_DIR,     15 },
    { "Cure",         GINSENG | GARLIC,         SC_ANY,      &spellCure,    SPELLPRM_PLAYER,  5 },
    { "Dispel",       ASH | GARLIC | PEARL,     SC_ANY,      &spellDispel,  SPELLPRM_DIR,     20 },
    { "Energy Field", ASH | SILK | PEARL,       SC_ANY,      &spellEField,  SPELLPRM_TYPEDIR, 10 },
    { "Fireball",     ASH | PEARL,              SC_COMBAT,   &spellFireball,SPELLPRM_DIR,     15 },
    { "Gate",         ASH | PEARL | MANDRAKE,   SC_NORMAL,   &spellGate,    SPELLPRM_PHASE,   /*40*/ 1 },
    { "Heal",         GINSENG | SILK,           SC_ANY,      &spellHeal,    SPELLPRM_PLAYER,  10 },
    { "Iceball",      PEARL | MANDRAKE,         SC_COMBAT,   &spellIceball, SPELLPRM_DIR,     20 },
    { "Jinx",         PEARL | NIGHTSHADE | MANDRAKE,
                                                SC_COMBAT,   &spellJinx,    SPELLPRM_NONE,    30 },
    { "Kill",         PEARL | NIGHTSHADE,       SC_COMBAT,   &spellKill,    SPELLPRM_DIR,     25 },
    { "Light",        ASH,                      SC_DUNGEON,  &spellLight,   SPELLPRM_NONE,    5 },
    { "Magic missile", ASH | PEARL,             SC_COMBAT,   &spellMMissle, SPELLPRM_DIR,     5 },
    { "Negate",       ASH | GARLIC | MANDRAKE,  SC_COMBAT,   &spellNegate,  SPELLPRM_NONE,    20 },
    { "Open",         ASH | MOSS,               SC_NORMAL,   &spellOpen,    SPELLPRM_NONE,    5 },
    { "Protection",   ASH | GINSENG | GARLIC,   SC_ANY,      &spellProtect, SPELLPRM_NONE,    15 },
    { "Quickness",    ASH | GINSENG | MOSS,     SC_ANY,      &spellQuick,   SPELLPRM_NONE,    20 },
    { "Resurrect",    ASH | GINSENG | GARLIC | SILK | MOSS | MANDRAKE, 
                                                SC_ANY,      &spellRez,     SPELLPRM_PLAYER,  45 },
    { "Sleep",        SILK | GINSENG,           SC_COMBAT,   &spellSleep,   SPELLPRM_NONE,    15 },
    { "Tremor",       ASH | MOSS | MANDRAKE,    SC_COMBAT,   &spellTremor,  SPELLPRM_NONE,    30 },
    { "Undead",       ASH | GARLIC,             SC_ANY,      &spellUndead,  SPELLPRM_NONE,    15 },
    { "View",         NIGHTSHADE | MANDRAKE,    SC_NORMAL | SC_DUNGEON, 
                                                             &spellView,    SPELLPRM_NONE,    15 },
    { "Winds",        ASH | MOSS,               SC_NORMAL,   &spellWinds,   SPELLPRM_FROMDIR, 10 },
    { "X-it",         ASH | SILK | MOSS,        SC_DUNGEON,  &spellXit,     SPELLPRM_NONE,    15 },
    { "Y-up",         SILK | MOSS,              SC_DUNGEON,  &spellYup,     SPELLPRM_NONE,    10 },
    { "Z-down",       SILK | MOSS,              SC_DUNGEON,  &spellZdown,   SPELLPRM_NONE,    5 }
};

#define N_SPELLS (sizeof(spells) / sizeof(spells[0]))

void playerSetSpellCallback(SpellCallback callback) {
    spellCallback = callback;    
}

Mixture *mixtureNew() {
    Mixture *mix = (Mixture *) malloc(sizeof(Mixture));
    memset(mix, 0, sizeof(Mixture));
    return mix;
}

void mixtureDelete(Mixture *mix) {
    memset(mix, 0, sizeof(Mixture));
    free(mix);
}

int mixtureAddReagent(Mixture *mix, Reagent reagent) {
    ASSERT(reagent < REAG_MAX, "invalid reagent: %d", reagent);
    if (c->saveGame->reagents[reagent] < 1)
        return 0;
    c->saveGame->reagents[reagent]--;
    mix->reagents[reagent]++;
    return 1;
}

void mixtureRevert(Mixture *mix) {
    int reg;

    for (reg = 0; reg < REAG_MAX; reg++) {
        c->saveGame->reagents[reg] += mix->reagents[reg];
        mix->reagents[reg] = 0;
    }
}

const char *spellGetName(unsigned int spell) {
    ASSERT(spell < N_SPELLS, "invalid spell: %d", spell);

    return spells[spell].name;
}

/**
 * Mix reagents for a spell.  Fails and returns false if the reagents
 * selected were not correct.
 */
int spellMix(unsigned int spell, const Mixture *mix) {
    int regmask, reg;

    ASSERT(spell < N_SPELLS, "invalid spell: %d", spell);

    regmask = 0;
    for (reg = 0; reg < REAG_MAX; reg++) {
        if (mix->reagents[reg] > 0)
            regmask |= (1 << reg);
    }

    if (regmask != spells[spell].components)
        return 0;

    c->saveGame->mixtures[spell]++;

    return 1;
}

SpellParam spellGetParamType(unsigned int spell) {
    ASSERT(spell < N_SPELLS, "invalid spell: %d", spell);

    return spells[spell].paramType;
}

/**
 * Casts spell.  Fails and returns false if no mixture is available,
 * the character doesn't have enough magic points, or the context is
 * invalid.  The error code is updated with the reason for failure.
 */
int spellCast(unsigned int spell, int character, int param, SpellCastError *error, int spellEffect) {
    int player = (spells[spell].paramType == SPELLPRM_PLAYER) ? param : -1;
    
    ASSERT(spell < N_SPELLS, "invalid spell: %d", spell);
    ASSERT(character >= 0 && character < c->saveGame->members, "character out of range: %d", character);

    *error = CASTERR_NOERROR;

    if (c->saveGame->mixtures[spell] == 0) {
        *error = CASTERR_NOMIX;
        return 0;
    }
        
    /* 
     * FIXME: handle dungeon and combat contexts when they are
     * implemented 
     */
    if ((spells[spell].context | SC_NORMAL) == 0) {
        *error = CASTERR_WRONGCONTEXT;
        return 0;
    }

    if (c->saveGame->players[character].mp < spells[spell].mp) {
        *error = CASTERR_MPTOOLOW;
        return 0;
    }

    c->saveGame->mixtures[spell]--;
    c->saveGame->players[character].mp -= spells[spell].mp;

    if (spellEffect)
        (*spellCallback)(spell + 'a', player);
    
    if (!(*spells[spell].spellFunc)(param)) {
        *error = CASTERR_FAILED;
        return 0;
    }    

    return 1;
}

void spellMagicAttack(unsigned char tile, int maxDamage, int minDamage) {
    int damage = 0;
    if (maxDamage > 0)
        damage = rand() % maxDamage;
    damage |= minDamage;
    printf("spell does %d damage\n", damage);
}

static int spellAwaken(int player) {
    ASSERT(player < 8, "player out of range: %d", player);

    if (player < c->saveGame->members && 
        c->saveGame->players[player].status == STAT_SLEEPING) {
        c->saveGame->players[player].status = STAT_GOOD;
        return 1;
    }

    return 0;
}

static int spellBlink(int dir) {
    /* blink doesn't work in lower right corner of map */
    if (c->saveGame->x >= 192 ||
        c->saveGame->y >= 192)
        return 0;

    /* FIXME */

    return 1;
}

static int spellCure(int player) {
    ASSERT(player < 8, "player out of range: %d", player);

    return playerHeal(c->saveGame, HT_CURE, player);
}

static int spellDispel(int dir) {
    int x, y, z;
    unsigned char tile;
    const Annotation *a;

    x = c->saveGame->x;
    y = c->saveGame->y;
    z = c->saveGame->dnglevel;
    dirMove((Direction) dir, &x, &y);
    if (MAP_IS_OOB(c->map, x, y))
        return 0;

    /*
     * if there is a field annotation, remove it
     */
    a = annotationAt(x, y, z, c->map->id);
    if (a && tileCanDispel(a->tile)) {
        annotationRemove(x, y, z, c->map->id, a->tile);
        return 1;
    }

    /*
     * if the map tile itself is a field, overlay it with a brick
     * annotation
     */
    tile = mapTileAt(c->map, x, y, z);
    
    if (!tileCanDispel(tile))
        return 0;

    annotationAdd(x, y, z, c->map->id, BRICKFLOOR_TILE);

    return 1;
}

static int spellEField(int dir) {
    int x, y, z;

    x = c->saveGame->x;
    y = c->saveGame->y;
    z = c->saveGame->dnglevel;
    dirMove((Direction) dir, &x, &y);
    if (MAP_IS_OOB(c->map, x, y))
        return 0;

    annotationAdd(x, y, z, c->map->id, LIGHTNINGFIELD_TILE);

    return 1;
}

static int spellFireball(int dir) {
    spellMagicAttack(HITFLASH_TILE, 128, 24);
    return 1;
}

static int spellGate(int phase) {
    const Moongate *moongate;

    moongate = moongateGetGateForPhase(phase);
    c->saveGame->x = moongate->x;
    c->saveGame->y = moongate->y;    
    return 1;
}

static int spellHeal(int player) {
    ASSERT(player < 8, "player out of range: %d", player);

    return playerHeal(c->saveGame, HT_HEAL, player);
}

static int spellIceball(int dir) {
    spellMagicAttack(MAGICFLASH_TILE, 224, 32);
    return 1;
}

static int spellJinx(int unused) {
    c->aura = AURA_JINX;
    c->auraDuration = 10;
    return 1;
}

static int spellKill(int dir) {
    spellMagicAttack(140, 0, 232);
    return 1;
}

static int spellLight(int unused) {
    c->saveGame->torchduration += 100;
    return 1;
}

static int spellMMissle(int dir) {
    spellMagicAttack(MISSFLASH_TILE, 64, 16);
    return 1;
}

static int spellNegate(int unused) {
    c->aura = AURA_NEGATE;
    c->auraDuration = 10;
    return 1;
}

static int spellOpen(int unused) {
    /* FIXME */
    return 1;
}

static int spellProtect(int unused) {
    c->aura = AURA_PROTECTION;
    c->auraDuration = 10;
    return 1;
}

static int spellRez(int player) {
    ASSERT(player < 8, "player out of range: %d", player);

    return playerHeal(c->saveGame, HT_RESURRECT, player);
}

static int spellQuick(int unused) {
    c->aura = AURA_QUICKNESS;
    c->auraDuration = 10;
    return 1;
}

static int spellSleep(int unused) {
    /* FIXME */
    return 1;
}

static int spellTremor(int unused) {
    /* FIXME */
    return 1;
}

static int spellUndead(int unused) {
    /* FIXME */
    return 1;
}

static int spellView(int unsued) {
    /* FIXME */
    return 1;
}

static int spellWinds(int fromdir) {
    c->windDirection = fromdir;
    return 1;
}

static int spellXit(int unused) {
    /* FIXME */
    return 1;
}

static int spellYup(int unused) {
    /* FIXME */
    return 1;
}

static int spellZdown(int unused) {
    /* FIXME */
    return 1;
}
