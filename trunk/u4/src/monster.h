/**
 * $Id$
 */

#ifndef MONSTER_H
#define MONSTER_H

#include "object.h"
#include "movement.h"

#define MAX_MONSTERS 128

/* Monsters on world map */

#define MAX_MONSTERS_ON_MAP 4
#define MAX_MONSTER_DISTANCE 10

/* Monster ids */

#define HORSE1_ID       0
#define HORSE2_ID       1

#define MAGE_ID         2
#define BARD_ID         3
#define FIGHTER_ID      4
#define DRUID_ID        5
#define TINKER_ID       6
#define PALADIN_ID      7
#define RANGER_ID       8
#define SHEPHERD_ID     9

#define GUARD_ID        10
#define VILLAGER_ID     11
#define SINGINGBARD_ID  12
#define JESTER_ID       13
#define BEGGAR_ID       14
#define CHILD_ID        15
#define BULL_ID         16
#define LORDBRITISH_ID  17

#define PIRATE_ID       18
#define NIXIE_ID        19
#define GIANT_SQUID_ID  20
#define SEA_SERPENT_ID  21
#define SEAHORSE_ID     22
#define WHIRLPOOL_ID    23
#define STORM_ID        24
#define RAT_ID          25
#define BAT_ID          26
#define GIANT_SPIDER_ID 27
#define GHOST_ID        28
#define SLIME_ID        29
#define TROLL_ID        30
#define GREMLIN_ID      31
#define MIMIC_ID        32
#define REAPER_ID       33
#define INSECT_SWARM_ID 34
#define GAZER_ID        35
#define PHANTOM_ID      36
#define ORC_ID          37
#define SKELETON_ID     38
#define ROGUE_ID        39
#define PYTHON_ID       40
#define ETTIN_ID        41
#define HEADLESS_ID     42
#define CYCLOPS_ID      43
#define WISP_ID         44
#define EVILMAGE_ID     45
#define LICH_ID         46
#define LAVA_LIZARD_ID  47
#define ZORN_ID         48
#define DAEMON_ID       49
#define HYDRA_ID        50
#define DRAGON_ID       51
#define BALRON_ID       52

typedef enum {
    MATTR_STEALFOOD     = 0x1,
    MATTR_STEALGOLD     = 0x2,
    MATTR_CASTS_SLEEP   = 0x4,
    MATTR_UNDEAD        = 0x8,
    MATTR_GOOD          = 0x10,
    MATTR_WATER         = 0x20,
    MATTR_NONATTACKABLE = 0x40,
    MATTR_NEGATE        = 0x80,    
    MATTR_CAMOUFLAGE    = 0x100,    
    MATTR_NOATTACK      = 0x200,    
    MATTR_AMBUSHES      = 0x400,
    MATTR_RANDOMRANGED  = 0x800,
    MATTR_INCORPOREAL   = 0x1000,
    MATTR_NOCHEST       = 0x2000,
    MATTR_DIVIDES       = 0x4000
} MonsterAttrib;

typedef enum {
    MATTR_STATIONARY        = 0x1,
    MATTR_WANDERS           = 0x2,
    MATTR_SWIMS             = 0x4,
    MATTR_SAILS             = 0x8,
    MATTR_FLIES             = 0x10,
    MATTR_TELEPORT          = 0x20,
    MATTR_CANMOVEMONSTERS   = 0x40,
    MATTR_CANMOVEAVATAR     = 0x80,
    MATTR_CANMOVEON         = 0x100
} MonsterMovementAttrib;

typedef enum {
    MSTAT_DEAD,
    MSTAT_FLEEING,
    MSTAT_CRITICAL,
    MSTAT_HEAVILYWOUNDED,
    MSTAT_LIGHTLYWOUNDED,
    MSTAT_BARELYWOUNDED
} MonsterStatus;

typedef struct _Monster {
    const char *name;
    unsigned short id;
    unsigned char tile;
    unsigned char camouflageTile;
    unsigned char frames;
    unsigned short leader;
    unsigned char basehp;
    unsigned short xp;
    unsigned char ranged;
    unsigned char worldrangedtile;
    unsigned char rangedhittile;
    unsigned char rangedmisstile;
    unsigned char leavestile        : 1;
    MonsterAttrib mattr;
    MonsterMovementAttrib movementAttr;
    SlowedType slowedType;
    unsigned char encounterSize;
    unsigned char resists;
} Monster;

const Monster *monsterForTile(unsigned char tile);

int monsterIsGood(const Monster *monster);
int monsterIsEvil(const Monster *monster);
int monsterIsUndead(const Monster *monster);
int monsterLeavesChest(const Monster *monster);
int monsterIsAquatic(const Monster *monster);
int monsterWanders(const Monster *monster);
int monsterIsStationary(const Monster *monster);
int monsterFlies(const Monster *monster);
int monsterTeleports(const Monster *monster);
int monsterSwims(const Monster *monster);
int monsterSails(const Monster *monster);
int monsterWalks(const Monster *monster);
int monsterDivides(const Monster *monster);
int monsterCanMoveOntoMonsters(const Monster *monster);
int monsterCanMoveOntoAvatar(const Monster *monster);
int monsterCanMoveOnto(const Monster *monster);
int monsterIsAttackable(const Monster *monster);
int monsterWillAttack(const Monster *monster);
int monsterStealsGold(const Monster *monster);
int monsterStealsFood(const Monster *monster);
int monsterNegates(const Monster *monster);
int monsterCamouflages(const Monster *monster);
int monsterAmbushes(const Monster *monster);
int monsterIsIncorporeal(const Monster *monster);
int monsterHasRandomRangedAttack(const Monster *monster);
int monsterLeavesTile(const Monster *monster);
int monsterCastSleep(const Monster *monster);
int monsterGetDamage(const Monster *monster);
int monsterGetCamouflageTile(const Monster *monster);
void monsterSetRandomRangedWeapon(Monster *monster);
const Monster *monsterRandomForTile(unsigned char tile);
const Monster *monsterForDungeon(int dngLevel);
int monsterGetInitialHp(const Monster *monster);
MonsterStatus monsterGetStatus(const Monster *monster, int hp);
const Monster *monsterGetAmbushingMonster(void);
int monsterSpecialAction(Object *obj);
void monsterSpecialEffect(Object *obj);
const Monster *monsterById(unsigned short id);

#endif