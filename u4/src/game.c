/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#include "u4.h"
#include "direction.h"
#include "game.h"
#include "screen.h"
#include "event.h"
#include "map.h"
#include "city.h"
#include "portal.h"
#include "person.h"
#include "ttype.h"
#include "context.h"
#include "annotation.h"
#include "savegame.h"
#include "stats.h"
#include "spell.h"
#include "names.h"
#include "player.h"
#include "moongate.h"
#include "music.h"
#include "item.h"
#include "shrine.h"
#include "death.h"
#include "combat.h"
#include "monster.h"
#include "camp.h"
#include "settings.h"
#include "error.h"

void gameLostEighth(Virtue virtue);
void gameAdvanceLevel(const SaveGamePlayerRecord *player);
void gameCastSpell(unsigned int spell, int caster, int param);
int gameCheckPlayerDisabled(int player);
void gameGetPlayerForCommand(int (*commandFn)(int player));
int moveAvatar(Direction dir, int userEvent);
int attackAtCoord(int x, int y, int distance);
int castForPlayer(int player);
int castForPlayer2(int spell, void *data);
int castForPlayerGetDestPlayer(int player);
int castForPlayerGetDestDir(Direction dir);
int fireAtCoord(int x, int y, int distance);
int jimmyAtCoord(int x, int y, int distance);
int mixReagentsForSpell(int spell, void *data);
int mixReagentsForSpell2(char choice);
int newOrderForPlayer(int player);
int newOrderForPlayer2(int player2);
int openAtCoord(int x, int y, int distance);
int gemHandleChoice(char choice);
int quitHandleChoice(char choice);
int readyForPlayer(int player);
int readyForPlayer2(int weapon, void *data);
int talkAtCoord(int x, int y, int distance);
void talkSetHandler(const Conversation *cnv);
int talkHandleBuffer(const char *message);
int talkHandleChoice(char choice);
int useItem(const char *itemName);
int wearForPlayer(int player);
int wearForPlayer2(int armor, void *data);
void gameCheckBridgeTrolls(void);
void gameCheckSpecialMonsters(Direction dir);
void gameCheckMoongates(void);
void gameCheckRandomMonsters(void);
long gameTimeSinceLastCommand(void);
int gameWindSlowsShip(Direction shipdir);

extern Map world_map;
Context *c = NULL;
int collisionOverride = 0;
ViewMode viewMode = VIEW_NORMAL;
char itemNameBuffer[16];

void gameInit() {
    FILE *saveGameFile, *monstersFile;

    /* initialize the global game context */
    c = (Context *) malloc(sizeof(Context));
    c->saveGame = (SaveGame *) malloc(sizeof(SaveGame));
    c->parent = NULL;
    c->map = &world_map;
    c->map->annotation = NULL;
    c->conversation.talker = NULL;
    c->conversation.state = 0;
    c->conversation.buffer[0] = '\0';
    c->line = TEXT_AREA_H - 1;
    c->col = 0;
    c->statsItem = STATS_PARTY_OVERVIEW;
    c->moonPhase = 0;
    c->windDirection = DIR_NORTH;
    c->windCounter = 0;
    c->aura = AURA_NONE;
    c->auraDuration = 0;
    c->horseSpeed = 0;
    c->lastCommandTime = time(NULL);

    /* load in the save game */
    saveGameFile = fopen("party.sav", "rb");
    if (saveGameFile) {
        saveGameRead(c->saveGame, saveGameFile);
        fclose(saveGameFile);
    } else
        errorFatal("no savegame found!");

    monstersFile = fopen("monsters.sav", "rb");
    if (monstersFile) {
        saveGameMonstersRead(&c->map->objects, monstersFile);
        fclose(monstersFile);
    }

    playerSetLostEighthCallback(&gameLostEighth);
    playerSetAdvanceLevelCallback(&gameAdvanceLevel);
    playerSetItemStatsChangedCallback(&statsUpdate);

    musicPlay();
    screenDrawBackground(BKGD_BORDERS);
    statsUpdate();
    screenMessage("\020");
}

/**
 * Sets the view mode.
 */
void gameSetViewMode(ViewMode newMode) {
    viewMode = newMode;
}

void gameUpdateScreen() {
    switch (viewMode) {
    case VIEW_NORMAL:
        screenUpdate(1, 0);
        break;
    case VIEW_GEM:
        screenGemUpdate();
        break;
    case VIEW_RUNE:
        screenUpdate(0, 0);
        break;
    case VIEW_DEAD:
        screenUpdate(1, 1);
        break;
    default:
        assert(0);              /* shouldn't happen */
    }
}

void gameSetMap(Context *ct, Map *map, int setStartPos) {
    int i;

    ct->map = map;
    ct->map->annotation = NULL;
    if (setStartPos) {
        ct->saveGame->dngx = ct->parent->saveGame->x;
        ct->saveGame->dngy = ct->parent->saveGame->y;
        ct->saveGame->x = map->startx;
        ct->saveGame->y = map->starty;
    }

    if ((map->type == MAP_TOWN || 
         map->type == MAP_VILLAGE ||
         map->type == MAP_CASTLE ||
         map->type == MAP_RUIN) && 
        map->objects == NULL) {
        for (i = 0; i < map->city->n_persons; i++) {
            if (map->city->persons[i].tile0 != 0 &&
                !(playerCanPersonJoin(c->saveGame, map->city->persons[i].name, NULL) && 
                  playerIsPersonJoined(c->saveGame, map->city->persons[i].name)))
                mapAddPersonObject(map, &(map->city->persons[i]));
        }
    }
}

/**
 * Terminates a game turn.  This performs the post-turn housekeeping
 * tasks like adjusting the party's food, incrementing the number of
 * moves, etc.
 */
void gameFinishTurn() {

    /* apply effects from tile avatar is standing on */
    playerApplyEffect(c->saveGame, tileGetEffect(mapTileAt(c->map, c->saveGame->x, c->saveGame->y)));

    while (1) {
        /* adjust food and moves */
        playerEndTurn(c->saveGame);

        /* check if aura has expired */
        if (c->auraDuration > 0) {
            if (--c->auraDuration == 0)
                c->aura = AURA_NONE;
        }

        mapMoveObjects(c->map, c->saveGame->x, c->saveGame->y);

        /* update map annotations and the party stats */
        annotationCycle();
        c->statsItem = STATS_PARTY_OVERVIEW;
        statsUpdate();

        gameCheckRandomMonsters();

        if (!playerPartyImmobilized(c->saveGame))
            break;

        if (playerPartyDead(c->saveGame)) {
            deathStart();
            return;
        } else {
            screenMessage("Zzzzzz\n");
        }
    }

    /* draw a prompt */
    screenMessage("\020");
    screenRedrawTextArea(TEXT_AREA_X, TEXT_AREA_Y, TEXT_AREA_W, TEXT_AREA_H);

    c->lastCommandTime = time(NULL);
}

/**
 * Inform a player he has lost zero or more eighths of avatarhood.
 */
void gameLostEighth(Virtue virtue) {
    screenMessage("Thou hast lost an eighth!\n");
    statsUpdate();
}

void gameAdvanceLevel(const SaveGamePlayerRecord *player) {
    screenMessage("%s\nThou art now Level %d\n", player->name, playerGetRealLevel(player));
    /* FIXME: special effect here */
    statsUpdate();
}

Context *gameCloneContext(Context *ctx) {
    Context *newContext;

    newContext = (Context *) malloc(sizeof(Context));
    newContext->parent = ctx;
    newContext->saveGame = newContext->parent->saveGame;
    newContext->col = newContext->parent->col;
    newContext->line = newContext->parent->line;
    newContext->statsItem = newContext->parent->statsItem;
    newContext->moonPhase = newContext->parent->moonPhase;
    newContext->windDirection = newContext->parent->windDirection;
    newContext->windCounter = newContext->parent->windCounter;
    newContext->moonPhase = newContext->parent->moonPhase;
    newContext->aura = newContext->parent->aura;
    newContext->auraDuration = newContext->parent->auraDuration;
    newContext->horseSpeed = newContext->parent->horseSpeed;
    newContext->horseSpeed = newContext->parent->horseSpeed;

    return newContext;
}

void gameCastSpell(unsigned int spell, int caster, int param) {
    SpellCastError spellError;
    int i;
    const char *msg = NULL;
    static const struct {
        SpellCastError err;
        const char *msg;
    } errorMsgs[] = {
        { CASTERR_NOMIX, "None Mixed!\n" },
        { CASTERR_WRONGCONTEXT, "Can't Cast Here!\n" },
        { CASTERR_MPTOOLOW, "Not Enough MP!\n" },
        { CASTERR_FAILED, "Failed!\n" }
    };

    if (!spellCast(spell, caster, param, &spellError)) {
        for (i = 0; i < sizeof(errorMsgs) / sizeof(errorMsgs[0]); i++) {
            if (spellError == errorMsgs[i].err) {
                msg = errorMsgs[i].msg;
                break;
            }
        }
        if (msg)
            screenMessage(msg);
    }
}

int gameCheckPlayerDisabled(int player) {
    assert(player < c->saveGame->members);

    if (c->saveGame->players[player].status == STAT_DEAD ||
        c->saveGame->players[player].status == STAT_SLEEPING) {
        screenMessage("Disabled!\n");
        return 1;
    }

    return 0;
}


/**
 * The main key handler for the game.  Interpretes each key as a
 * command - 'a' for attack, 't' for talk, etc.
 */
int gameBaseKeyHandler(int key, void *data) {
    int valid = 1;
    Object *obj;
    const Portal *portal;
    CoordActionInfo *info;
    GetChoiceActionInfo *choiceInfo;
    AlphaActionInfo *alphaInfo;
    ReadBufferActionInfo *readBufferInfo;
    const ItemLocation *item;
    unsigned char tile;

    switch (key) {

    case U4_UP:
        moveAvatar(DIR_NORTH, 1);
        break;

    case U4_DOWN:
        moveAvatar(DIR_SOUTH, 1);
        break;

    case U4_LEFT:
        moveAvatar(DIR_WEST, 1);
        break;

    case U4_RIGHT:
        moveAvatar(DIR_EAST, 1);
        break;

    case 3:                     /* ctrl-C */
        screenMessage("Cmd: ");
        eventHandlerPushKeyHandler(&gameSpecialCmdKeyHandler);
        break;

    case ' ':
        screenMessage("Pass\n");
        break;

    case 'a':
        info = (CoordActionInfo *) malloc(sizeof(CoordActionInfo));
        info->handleAtCoord = &attackAtCoord;
        info->origin_x = c->saveGame->x;
        info->origin_y = c->saveGame->y;
        info->range = 1;
        info->validDirections = MASK_DIR_ALL;
        info->blockedPredicate = NULL;
        eventHandlerPushKeyHandlerData(&gameGetCoordinateKeyHandler, info);
        screenMessage("Attack\nDir: ");
        break;

    case 'b':
        obj = mapObjectAt(c->map, c->saveGame->x, c->saveGame->y);
        if (obj) {
            if (tileIsShip(obj->tile)) {
                if (c->saveGame->transport != AVATAR_TILE)
                    screenMessage("Board: Can't!\n");
                else {
                    c->saveGame->transport = obj->tile;
                    c->saveGame->shiphull = 50;
                    mapRemoveObject(c->map, obj);
                    screenMessage("Board Frigate!\n");
                }
            } else if (tileIsHorse(obj->tile)) {
                if (c->saveGame->transport != AVATAR_TILE)
                    screenMessage("Board: Can't!\n");
                else {
                    c->saveGame->transport = obj->tile;
                    mapRemoveObject(c->map, obj);
                    screenMessage("Mount Horse!\n");
                }
            } else if (tileIsBalloon(obj->tile)) {
                if (c->saveGame->transport != AVATAR_TILE)
                    screenMessage("Board: Can't!\n");
                else {
                    c->saveGame->transport = obj->tile;
                    mapRemoveObject(c->map, obj);
                    screenMessage("Board Balloon!\n");
                }
            }
        } else
            screenMessage("Board What?\n");
        break;

    case 'c':
        screenMessage("Cast Spell!\nPlayer: ");
        gameGetPlayerForCommand(&castForPlayer);
        break;

    case 'd':
        portal = mapPortalAt(c->map, c->saveGame->x, c->saveGame->y);
        if (portal && portal->trigger_action == ACTION_DESCEND) {
            gameSetMap(c, portal->destination, 0);
            screenMessage("Descend to first floor!\n");
        } else if (tileIsBalloon(c->saveGame->transport)) {
            screenMessage("Land Balloon\n");
            if (c->saveGame->balloonstate == 0)
                screenMessage("Already Landed!\n");
            c->saveGame->balloonstate = 0;
        } else
            screenMessage("Descend what?\n");
        break;

    case 'e':
        portal = mapPortalAt(c->map, c->saveGame->x, c->saveGame->y);
        if (portal && portal->trigger_action == ACTION_ENTER) {
            switch (portal->destination->type) {
            case MAP_TOWN:
                screenMessage("Enter towne!\n\n%s\n\n", portal->destination->city->name);
                break;
            case MAP_VILLAGE:
                screenMessage("Enter village!\n\n%s\n\n", portal->destination->city->name);
                break;
            case MAP_CASTLE:
                screenMessage("Enter castle!\n\n%s\n\n", portal->destination->city->name);
                break;
            case MAP_RUIN:
                screenMessage("Enter ruin!\n\n%s\n\n", portal->destination->city->name);
                break;
            case MAP_SHRINE:
                screenMessage("Enter the Shrine of %s!\n\n", getVirtueName(portal->destination->shrine->virtue));
                break;
            default:
                break;
            }

            /*
             * if trying to enter a shrine, ensure the player is
             * allowed in
             */
            if (portal->destination->type == MAP_SHRINE) {
                if (!playerCanEnterShrine(c->saveGame, portal->destination->shrine->virtue)) {
                    screenMessage("Thou dost not bear the rune of entry!  A strange force keeps you out!\n");
                    break;
                } else {
                    shrineEnter(portal->destination->shrine);
                }
            }

            annotationClear();  /* clear out world map annotations */

            c = gameCloneContext(c);

            gameSetMap(c, portal->destination, 1);
            musicPlay();

        } else
            screenMessage("Enter what?\n");
        break;

    case 'f':
        if (tileIsShip(c->saveGame->transport)) {
            int validDirs;
            validDirs = DIR_REMOVE_FROM_MASK(tileGetDirection(c->saveGame->transport), MASK_DIR_ALL);
            validDirs = DIR_REMOVE_FROM_MASK(dirReverse(tileGetDirection(c->saveGame->transport)), validDirs);

            info = (CoordActionInfo *) malloc(sizeof(CoordActionInfo));
            info->handleAtCoord = &fireAtCoord;
            info->origin_x = c->saveGame->x;
            info->origin_y = c->saveGame->y;
            info->range = 1;
            info->validDirections = validDirs;
            info->blockedPredicate = NULL;
            eventHandlerPushKeyHandlerData(&gameGetCoordinateKeyHandler, info);

            printf("validDirs = %d\n", validDirs);
            screenMessage("Fire Cannon!\nDir: ");
        }
        else
            screenMessage("Fire What?\n");
        break;

    case 'g':
        screenMessage("Get Chest!\n");
        if ((obj = mapObjectAt(c->map, c->saveGame->x, c->saveGame->y)) != NULL)
            tile = obj->tile;
        else
            tile = mapTileAt(c->map, c->saveGame->x, c->saveGame->y);
        if (tile == CHEST_TILE) {
            if (obj)
                mapRemoveObject(c->map, obj);
            else
                annotationAdd(c->saveGame->x, c->saveGame->y, BRICKFLOOR_TILE);
            screenMessage("The Chest Holds: %d Gold\n", playerGetChest(c->saveGame));
            if (obj == NULL)
                playerAdjustKarma(c->saveGame, KA_STOLE_CHEST);
        } else
            screenMessage("Not Here!\n");
        break;

    case 'h':
        if (!mapIsWorldMap(c->map)) {
            screenMessage("Hole up & Camp\nNot here!\n");
            break;
        }
        if (c->saveGame->transport != AVATAR_TILE) {
            screenMessage("Hole up & Camp\nOnly on foot!\n");
            break;
        }
        screenMessage("Hole up & Camp!\n");
        campBegin();
        break;

    case 'i':
        screenMessage("Ignite torch!\nNot Here!\n");
        break;

    case 'j':
        info = (CoordActionInfo *) malloc(sizeof(CoordActionInfo));
        info->handleAtCoord = &jimmyAtCoord;
        info->origin_x = c->saveGame->x;
        info->origin_y = c->saveGame->y;
        info->range = 1;
        info->validDirections = MASK_DIR_ALL;
        info->blockedPredicate = NULL;
        eventHandlerPushKeyHandlerData(&gameGetCoordinateKeyHandler, info);
        screenMessage("Jimmy\nDir: ");
        break;
        
    case 'k':
        portal = mapPortalAt(c->map, c->saveGame->x, c->saveGame->y);
        if (portal && portal->trigger_action == ACTION_KLIMB) {
            if (c->saveGame->transport != AVATAR_TILE)
                screenMessage("Klimb\nOnly on foot!\n");
            else {
                gameSetMap(c, portal->destination, 0);
                screenMessage("Klimb to second floor!\n");
            }
        } else if (tileIsBalloon(c->saveGame->transport)) {
            c->saveGame->balloonstate = 1;
            screenMessage("Klimb Altitude!\n");
        } else
            screenMessage("Klimb what?\n");
        break;

    case 'l':
        if (c->saveGame->sextants >= 1)
            screenMessage("Locate position\nwith sextant\n Latitude: %c'%c\"\nLongitude: %c'%c\"\n",
                          c->saveGame->y / 16 + 'A', c->saveGame->y % 16 + 'A',
                          c->saveGame->x / 16 + 'A', c->saveGame->x % 16 + 'A');
        else
            screenMessage("Locate position\nwith what?\n");
        break;

    case 'm':
        screenMessage("Mix reagents\n");
        alphaInfo = (AlphaActionInfo *) malloc(sizeof(AlphaActionInfo));
        alphaInfo->lastValidLetter = 'z';
        alphaInfo->handleAlpha = mixReagentsForSpell;
        alphaInfo->prompt = "For Spell: ";
        alphaInfo->data = NULL;

        screenMessage("%s", alphaInfo->prompt);
        eventHandlerPushKeyHandlerData(&gameGetAlphaChoiceKeyHandler, alphaInfo);

        c->statsItem = STATS_MIXTURES;
        statsUpdate();
        break;

    case 'n':
        eventHandlerPushKeyHandlerData(&gameGetPlayerNoKeyHandler, (void *) &newOrderForPlayer);
        screenMessage("New Order!\nExchange # ");
        break;

    case 'o':
        info = (CoordActionInfo *) malloc(sizeof(CoordActionInfo));
        info->handleAtCoord = &openAtCoord;
        info->origin_x = c->saveGame->x;
        info->origin_y = c->saveGame->y;
        info->range = 1;
        info->validDirections = MASK_DIR_ALL;
        info->blockedPredicate = NULL;
        eventHandlerPushKeyHandlerData(&gameGetCoordinateKeyHandler, info);
        screenMessage("Open\nDir: ");
        break;

    case 'p':
        if (c->saveGame->gems) {
            c->saveGame->gems--;
            viewMode = VIEW_GEM;
            choiceInfo = (GetChoiceActionInfo *) malloc(sizeof(GetChoiceActionInfo));
            choiceInfo->choices = " \033";
            choiceInfo->handleChoice = &gemHandleChoice;
            eventHandlerPushKeyHandlerData(&keyHandlerGetChoice, choiceInfo);
            screenMessage("Peer at a Gem!\n");
        } else
            screenMessage("Peer at What?\n");
        break;

    case 'q':
        if (!mapIsWorldMap(c->map)) {
            screenMessage("Quit & save\nNot Here!\n");
        } else {
            choiceInfo = (GetChoiceActionInfo *) malloc(sizeof(GetChoiceActionInfo));
            choiceInfo->choices = "yn";
            choiceInfo->handleChoice = &quitHandleChoice;
            eventHandlerPushKeyHandlerData(&keyHandlerGetChoice, choiceInfo);
            screenMessage("Quit & Save...\n%d moves\nExit (Y/N)? ", c->saveGame->moves);
        }
        break;

    case 'r':
        screenMessage("Ready a weapon\nfor: ");
        gameGetPlayerForCommand(&readyForPlayer);
        break;

    case 's':
        screenMessage("Searching...\n");
        item = itemAtLocation(c->map, c->saveGame->x, c->saveGame->y);
        if (item) {
            if ((*item->isItemInInventory)(item->data))
                screenMessage("Nothing Here!\n");
            else {
                (*item->putItemInInventory)(item->data);
                screenMessage("You find...\n%s!\n", item->name);
            }
        } else
            screenMessage("Nothing Here!\n");
        break;

    case 't':
        info = (CoordActionInfo *) malloc(sizeof(CoordActionInfo));
        info->handleAtCoord = &talkAtCoord;
        info->origin_x = c->saveGame->x;
        info->origin_y = c->saveGame->y;
        info->range = 2;
        info->validDirections = MASK_DIR_ALL;
        info->blockedPredicate = &tileCanTalkOver;
        eventHandlerPushKeyHandlerData(&gameGetCoordinateKeyHandler, info);
        screenMessage("Talk\nDir: ");
        break;

    case 'u':
        screenMessage("Use which item:\n");
        readBufferInfo = (ReadBufferActionInfo *) malloc(sizeof(ReadBufferActionInfo));
        readBufferInfo->handleBuffer = &useItem;
        readBufferInfo->buffer = itemNameBuffer;
        readBufferInfo->bufferLen = sizeof(itemNameBuffer);
        readBufferInfo->screenX = TEXT_AREA_X + c->col;
        readBufferInfo->screenY = TEXT_AREA_Y + c->line;
        itemNameBuffer[0] = '\0';
        eventHandlerPushKeyHandlerData(&keyHandlerReadBuffer, readBufferInfo);
        break;

    case 'v':
        if (musicToggle())
            screenMessage("Volume On!\n");
        else
            screenMessage("Volume Off!\n");
        break;

    case 'w':
        screenMessage("Wear Armor\nfor: ");
        gameGetPlayerForCommand(&wearForPlayer);
        break;

    case 'x':
        if (c->saveGame->transport != AVATAR_TILE && c->saveGame->balloonstate == 0) {
            mapAddObject(c->map, c->saveGame->transport, c->saveGame->transport, c->saveGame->x, c->saveGame->y);
            c->saveGame->transport = AVATAR_TILE;
            c->horseSpeed = 0;
            screenMessage("X-it\n");
        } else
            screenMessage("X-it What?\n");
        break;

    case 'y':
        screenMessage("Yell ");
        if (tileIsHorse(c->saveGame->transport)) {
            if (c->horseSpeed == 0) {
                screenMessage("Giddyup!\n");
                c->horseSpeed = 1;
            } else {
                screenMessage("Whoa!\n");
                c->horseSpeed = 0;
            }
        } else
            screenMessage("what?\n");
        break;

    case 'z':
        eventHandlerPushKeyHandler(&gameZtatsKeyHandler);
        screenMessage("Ztats for: ");
        break;

    case 'v' + U4_ALT:
        screenMessage("XU4 %s\n", VERSION);
        break;

    case 'x' + U4_ALT:
        eventHandlerSetExitFlag(1);
        break;

    default:
        valid = 0;
        break;
    }

    if (valid) {
        if (eventHandlerGetKeyHandler() == &gameBaseKeyHandler)
            gameFinishTurn();
    }

    return valid || keyHandlerDefault(key, NULL);
}

void gameGetPlayerForCommand(int (*commandFn)(int player)) {
    if (c->saveGame->members <= 1) {
        screenMessage("1\n");
        (*commandFn)(0);
    } else
        eventHandlerPushKeyHandlerData(&gameGetPlayerNoKeyHandler, (void *) commandFn);
}

/**
 * Handles key presses for a command requiring a player number
 * argument.  Once a number key is pressed, control is handed off to a
 * command specific routine.
 */
int gameGetPlayerNoKeyHandler(int key, void *data) {
    int (*handlePlayerNo)(int player) = (int(*)(int))data;
    int valid = 1;

    eventHandlerPopKeyHandler();

    if (key >= '1' &&
        key <= ('0' + c->saveGame->members)) {
        screenMessage("%c\n", key);
        (*handlePlayerNo)(key - '1');
    } else {
        screenMessage("None\n");
        gameFinishTurn();
        valid = 0;
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses for a command requiring a letter argument.
 * Once a valid key is pressed, control is handed off to a command
 * specific routine.
 */
int gameGetAlphaChoiceKeyHandler(int key, void *data) {
    AlphaActionInfo *info = (AlphaActionInfo *) data;
    int valid = 1;


    if (isupper(key))
        key = tolower(key);

    if (key >= 'a' && key <= info->lastValidLetter) {
        screenMessage("%c\n", key);
        eventHandlerPopKeyHandler();
        (*(info->handleAlpha))(key - 'a', info->data);
        free(info);
    } else if (key == ' ' || key == U4_ESC) {
        screenMessage("\n");
        eventHandlerPopKeyHandler();
        free(info);
        gameFinishTurn();
    } else {
        valid = 0;
        screenMessage("\n%s", info->prompt);
        screenRedrawScreen();
    }

    return valid || keyHandlerDefault(key, NULL);
}

int gameGetDirectionKeyHandler(int key, void *data) {
    int (*handleDirection)(Direction dir) = (int(*)(Direction))data;
    int valid = 1;
    Direction dir;

    switch (key) {
    case U4_UP:
        dir = DIR_NORTH;
        break;
    case U4_DOWN:
        dir = DIR_SOUTH;
        break;
    case U4_LEFT:
        dir = DIR_WEST;
        break;
    case U4_RIGHT:
        dir = DIR_EAST;
        break;
    default:
        valid = 0;
        break;
    }

    if (valid) {
        eventHandlerPopKeyHandler();

        screenMessage("%s\n", getDirectionName(dir));
        (*handleDirection)(dir);
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses for a command requiring a direction argument.
 * Once an arrow key is pressed, control is handed off to a command
 * specific routine.
 */
int gameGetCoordinateKeyHandler(int key, void *data) {
    CoordActionInfo *info = (CoordActionInfo *) data;
    int valid = 1;
    int distance = 0;
    Direction dir;
    int t_x = info->origin_x, t_y = info->origin_y;

    eventHandlerPopKeyHandler();

    switch (key) {
    case U4_UP:
        dir = DIR_NORTH;
        break;
    case U4_DOWN:
        dir = DIR_SOUTH;
        break;
    case U4_LEFT:
        dir = DIR_WEST;
        break;
    case U4_RIGHT:
        dir = DIR_EAST;
        break;
    default:
        valid = 0;
        break;
    }

    if (!valid || !DIR_IN_MASK(dir,info->validDirections))
        goto failure;
        
    screenMessage("%s\n", getDirectionName(dir));

    /* 
     * try every tile in the given direction, up to the given range.
     * Stop when the command handler succeeds, the range is exceeded,
     * or the action is blocked.
     */
    for (distance = 1; distance <= info->range; distance++) {
        dirMove(dir, &t_x, &t_y);

        if ((*(info->handleAtCoord))(t_x, t_y, distance))
            goto success;
        if (info->blockedPredicate &&
            !(*(info->blockedPredicate))(mapTileAt(c->map, t_x, t_y)))
            break;
    }

 failure:
    (*info->handleAtCoord)(-1, -1, distance);

 success:
    free(info);

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses when the Ztats prompt is active.
 */
int gameZtatsKeyHandler(int key, void *data) {
    int valid = 1;

    if (key == '0')
        c->statsItem = STATS_WEAPONS;
    else if (key >= '1' && key <= '8' && (key - '1' + 1) <= c->saveGame->members)
        c->statsItem = (StatsItem) (STATS_CHAR1 + (key - '1'));
    else if (key == '\033') {
        screenMessage("\n");
        eventHandlerPopKeyHandler();
        gameFinishTurn();
        return 1;
    }
    else
        valid = 0;

    if (valid) {
        screenMessage("%c\n", key);
        statsUpdate();
        eventHandlerPopKeyHandler();
        eventHandlerPushKeyHandler(&gameZtatsKeyHandler2);
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses while Ztats are being displayed.
 */
int gameZtatsKeyHandler2(int key, void *data) {
    switch (key) {
    case U4_UP:
    case U4_LEFT:
        c->statsItem = (StatsItem) (c->statsItem - 1);
        if (c->statsItem < STATS_CHAR1)
            c->statsItem = STATS_MIXTURES;
        if (c->statsItem <= STATS_CHAR8 &&
            (c->statsItem - STATS_CHAR1 + 1) > c->saveGame->members)
            c->statsItem = (StatsItem) (STATS_CHAR1 - 1 + c->saveGame->members);
        break;
    case U4_DOWN:
    case U4_RIGHT:
        c->statsItem = (StatsItem) (c->statsItem + 1);
        if (c->statsItem > STATS_MIXTURES)
            c->statsItem = STATS_CHAR1;
        if (c->statsItem <= STATS_CHAR8 &&
            (c->statsItem - STATS_CHAR1 + 1) > c->saveGame->members)
            c->statsItem = STATS_WEAPONS;
        break;
    default:
        eventHandlerPopKeyHandler();
        gameFinishTurn();
        break;
    }

    statsUpdate();

    return 1;
}

int gameSpecialCmdKeyHandler(int key, void *data) {
    int i;
    const Moongate *moongate;
    int valid = 1;

    switch (key) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
        moongate = moongateGetGateForPhase(key - '0');
        c->saveGame->x = moongate->x;
        c->saveGame->y = moongate->y;
        screenMessage("Gate %d!\n", key - '0');
        break;
        
    case 'c':
        collisionOverride = !collisionOverride;
        screenMessage("Collision detection %s!\n\020", collisionOverride ? "off" : "on");
        break;
    case 'e':
        screenMessage("Equipment!\n\020");
        for (i = ARMR_NONE + 1; i < ARMR_MAX; i++)
            c->saveGame->armor[i] = 8;
        for (i = WEAP_HANDS + 1; i < WEAP_MAX; i++)
            c->saveGame->weapons[i] = 8;
        break;
    case 'h':
        screenMessage("Help:\n"
                      "0-7 - gate to city\n"
                      "c - Collision\ne - Equipment\nh - Help\ni - Items\nk - Show Karma\n"
                      "m - Mixtures\nr - Reagents\nt - Transports\nw - Winds\n"
                      "\020");
        break;
    case 'i':
        screenMessage("Items!\n\020");
        c->saveGame->torches = 99;
        c->saveGame->gems = 99;
        c->saveGame->keys = 99;
        c->saveGame->sextants = 1;
        c->saveGame->items = ITEM_SKULL | ITEM_CANDLE | ITEM_BOOK | ITEM_BELL | ITEM_KEY_C | ITEM_KEY_L | ITEM_KEY_T | ITEM_HORN | ITEM_WHEEL;
        c->saveGame->stones = 0xff;
        c->saveGame->runes = 0xff;
        c->saveGame->food = 99900;
        c->saveGame->gold = 999;
        statsUpdate();
        break;
    case 'k':
        screenMessage("Karma:\nH C V J S H S H\n%02x%02x%02x%02x%02x%02x%02x%02x\n\020", c->saveGame->karma[0], c->saveGame->karma[1], c->saveGame->karma[2],
                      c->saveGame->karma[3], c->saveGame->karma[4], c->saveGame->karma[5], c->saveGame->karma[6], c->saveGame->karma[7]);
        break;
    case 'm':
        screenMessage("Mixtures!\n\020");
        for (i = 0; i < SPELL_MAX; i++)
            c->saveGame->mixtures[i] = 99;
        break;
    case 'r':
        screenMessage("Reagents!\n\020");
        for (i = 0; i < REAG_MAX; i++)
            c->saveGame->reagents[i] = 99;
        break;
    case 't':
        if (mapIsWorldMap(c->map)) {
            mapAddObject(c->map, tileGetHorseBase(), tileGetHorseBase(), 84, 106);
            mapAddObject(c->map, tileGetShipBase(), tileGetShipBase(), 88, 109);
            mapAddObject(c->map, tileGetBalloonBase(), tileGetBalloonBase(), 85, 105);
            screenMessage("Transports: Ship, Horse and Balloon created!\n\020");
        }
        break;
    case 'w':
        c->windDirection++;
        if (c->windDirection >= DIR_SOUTH)
            c->windDirection = DIR_WEST;
        screenMessage("Change Wind Direction\n");
        break;

    default:
        valid = 0;
        break;
    }

    if (valid)
        eventHandlerPopKeyHandler();

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Attempts to attack a creature at map coordinates x,y.  If no
 * creature is present at that point, zero is returned.
 */
int attackAtCoord(int x, int y, int distance) {
    Object *obj, *under;
    const Monster *m;
    unsigned char ground;

    /* attack failed: finish up */
    if (x == -1 && y == -1) {
        screenMessage("Attack What?\n");
        gameFinishTurn();
        return 0;
    }

    /* nothing attackable: move on to next tile */
    if ((obj = mapObjectAt(c->map, x, y)) == NULL ||
        (m = monsterForTile(obj->tile)) == NULL ||
        (m->mattr & MATTR_NONATTACKABLE)) {
        return 0;
    }

    /* attack successful */
    ground = mapTileAt(c->map, c->saveGame->x, c->saveGame->y);
    if ((under = mapObjectAt(c->map, c->saveGame->x, c->saveGame->y)) &&
        tileIsShip(under->tile))
        ground = under->tile;
    combatBegin(ground, c->saveGame->transport, obj);
    return 1;
}

int castPlayer;
unsigned int castSpell;

int castForPlayer(int player) {
    AlphaActionInfo *info;

    castPlayer = player;

    if (gameCheckPlayerDisabled(player)) {
        gameFinishTurn();
        return 0;
    }

    c->statsItem = STATS_MIXTURES;
    statsUpdate();

    info = (AlphaActionInfo *) malloc(sizeof(AlphaActionInfo));
    info->lastValidLetter = 'z';
    info->handleAlpha = castForPlayer2;
    info->prompt = "Spell: ";
    info->data = NULL;

    screenMessage("%s", info->prompt);

    eventHandlerPushKeyHandlerData(&gameGetAlphaChoiceKeyHandler, info);

    return 1;
}

int castForPlayer2(int spell, void *data) {
    castSpell = spell;

    screenMessage("%s!\n", spellGetName(spell));

    c->statsItem = STATS_PARTY_OVERVIEW;
    statsUpdate();

    switch (spellGetParamType(spell)) {
    case SPELLPRM_NONE:
    case SPELLPRM_PHASE:
        gameCastSpell(castSpell, castPlayer, 0);
        gameFinishTurn();
        break;
    case SPELLPRM_PLAYER:
        screenMessage("Player: ");
        eventHandlerPushKeyHandlerData(&gameGetPlayerNoKeyHandler, (void *) &castForPlayerGetDestPlayer);
        break;
    case SPELLPRM_DIR:
    case SPELLPRM_TYPEDIR:
        screenMessage("Dir: ");
        eventHandlerPushKeyHandlerData(&gameGetDirectionKeyHandler, (void *) &castForPlayerGetDestDir);
        break;
    case SPELLPRM_FROMDIR:
        screenMessage("From Dir: ");
        eventHandlerPushKeyHandlerData(&gameGetDirectionKeyHandler, (void *) &castForPlayerGetDestDir);
        break;
    }

    return 1;
}

int castForPlayerGetDestPlayer(int player) {
    gameCastSpell(castSpell, castPlayer, player);
    gameFinishTurn();
    return 1;
}

int castForPlayerGetDestDir(Direction dir) {
    gameCastSpell(castSpell, castPlayer, (int) dir);
    gameFinishTurn();
    return 1;
}

int fireAtCoord(int x, int y, int distance) {
    if (x == -1 && y == -1) {
        if (distance == 0)
            screenMessage("Broadsides Only!\n");
        else
            screenMessage("Missed!\n");
        gameFinishTurn();
        return 0;
    }
    
    screenMessage("Boom!\n");

    gameFinishTurn();

    return 1;
}

/**
 * Attempts to jimmy a locked door at map coordinates x,y.  The locked
 * door is replaced by a permanent annotation of an unlocked door
 * tile.
 */
int jimmyAtCoord(int x, int y, int distance) {
    if (x == -1 && y == -1) {
        screenMessage("Jimmy what?\n");
        gameFinishTurn();
        return 0;
    }
    
    if (!tileIsLockedDoor(mapTileAt(c->map, x, y)))
        return 0;

    if (c->saveGame->keys) {
        c->saveGame->keys--;
        annotationAdd(x, y, 0x3b);
        screenMessage("\nUnlocked!\n");
    } else
        screenMessage("No keys left!\n");

    gameFinishTurn();

    return 1;
}

/**
 * Readies a weapon for the given player.  Prompts the use for a
 * weapon.
 */
int readyForPlayer(int player) {
    AlphaActionInfo *info;

    c->statsItem = STATS_WEAPONS;
    statsUpdate();

    info = (AlphaActionInfo *) malloc(sizeof(AlphaActionInfo));
    info->lastValidLetter = WEAP_MAX + 'a' - 1;
    info->handleAlpha = readyForPlayer2;
    info->prompt = "Weapon: ";
    info->data = (void *) player;

    screenMessage("%s", info->prompt);

    eventHandlerPushKeyHandlerData(&gameGetAlphaChoiceKeyHandler, info);

    return 1;
}

int readyForPlayer2(int w, void *data) {
    int player = (int) data;
    WeaponType weapon = (WeaponType) w, oldWeapon;

    if (weapon != WEAP_HANDS && c->saveGame->weapons[weapon] < 1) {
        screenMessage("None left!\n");
        gameFinishTurn();
        return 0;
    }

    if (!playerCanReady(&(c->saveGame->players[player]), weapon)) {
        screenMessage("\nA %s may NOT\nuse\n%s\n", getClassName(c->saveGame->players[player].klass), getWeaponName(weapon));
        gameFinishTurn();
        return 0;
    }

    oldWeapon = c->saveGame->players[player].weapon;
    if (oldWeapon != WEAP_HANDS)
        c->saveGame->weapons[oldWeapon]++;
    if (weapon != WEAP_HANDS)
        c->saveGame->weapons[weapon]--;
    c->saveGame->players[player].weapon = weapon;

    screenMessage("%s\n", getWeaponName(weapon));

    gameFinishTurn();

    return 1;
}

/* FIXME */
Mixture *mix;
int mixSpell;

/**
 * Mixes reagents for a spell.  Prompts for reagents.
 */
int mixReagentsForSpell(int spell, void *data) {
    GetChoiceActionInfo *info;

    mixSpell = spell;
    mix = mixtureNew();

    screenMessage("%s\nReagent: ", spellGetName(spell));

    c->statsItem = STATS_REAGENTS;
    statsUpdate();

    info = (GetChoiceActionInfo *) malloc(sizeof(GetChoiceActionInfo));
    info->choices = "abcdefgh\n\r \033";
    info->handleChoice = &mixReagentsForSpell2;
    eventHandlerPushKeyHandlerData(&keyHandlerGetChoice, info);

    return 0;
}

int mixReagentsForSpell2(char choice) {
    GetChoiceActionInfo *info;
    AlphaActionInfo *alphaInfo;

    eventHandlerPopKeyHandler();

    if (choice == '\n' || choice == '\r' || choice == ' ') {
        screenMessage("\n\nYou mix the Reagents, and...\n");

        if (spellMix(mixSpell, mix))
            screenMessage("Success!\n\n");
        else
            screenMessage("It Fizzles!\n\n");

        mixtureDelete(mix);

        screenMessage("Mix reagents\n");
        alphaInfo = (AlphaActionInfo *) malloc(sizeof(AlphaActionInfo));
        alphaInfo->lastValidLetter = 'z';
        alphaInfo->handleAlpha = mixReagentsForSpell;
        alphaInfo->prompt = "For Spell: ";
        alphaInfo->data = NULL;

        screenMessage("%s", alphaInfo->prompt);
        eventHandlerPushKeyHandlerData(&gameGetAlphaChoiceKeyHandler, alphaInfo);

        c->statsItem = STATS_MIXTURES;
        statsUpdate();

        return 1;
    } 

    else if (choice == '\033') {

        mixtureRevert(mix);
        mixtureDelete(mix);

        screenMessage("\n\n");
        gameFinishTurn();
        return 1;
    } 

    else {
        screenMessage("%c\n", toupper(choice));

        if (mixtureAddReagent(mix, choice - 'a'))
            statsUpdate();
        else
            screenMessage("None Left!\n");

        screenMessage("Reagent: ");

        info = (GetChoiceActionInfo *) malloc(sizeof(GetChoiceActionInfo));
        info->choices = "abcdefgh\n\r \033";
        info->handleChoice = &mixReagentsForSpell2;
        eventHandlerPushKeyHandlerData(&keyHandlerGetChoice, info);


        return 1;
    }
}

/* FIXME: must be a better way.. */
int newOrderTemp;

/**
 * Exchanges the position of two players in the party.  Prompts the
 * use for the second player number.
 */
int newOrderForPlayer(int player) {
    if (player == 0) {
        screenMessage("%s, You must\nlead!\n", c->saveGame->players[0].name);
        gameFinishTurn();
        return 0;
    }

    screenMessage("    with # ");

    newOrderTemp = player;
    eventHandlerPushKeyHandlerData(&gameGetPlayerNoKeyHandler, (void *) &newOrderForPlayer2);

    return 1;
}

int newOrderForPlayer2(int player2) {
    int player1 = newOrderTemp;
    SaveGamePlayerRecord tmp;

    if (player2 == 0) {
        screenMessage("%s, You must\nlead!\n", c->saveGame->players[0].name);
        gameFinishTurn();
        return 0;
    } else if (player1 == player2) {
        gameFinishTurn();
        return 0;
    }

    tmp = c->saveGame->players[player1];
    c->saveGame->players[player1] = c->saveGame->players[player2];
    c->saveGame->players[player2] = tmp;

    statsUpdate();

    return 1;
}

/**
 * Attempts to open a door at map coordinates x,y.  The door is
 * replaced by a temporary annotation of a floor tile for 4 turns.
 */
int openAtCoord(int x, int y, int distance) {
    if (x == -1 && y == -1) {
        screenMessage("Not Here!\n");
        gameFinishTurn();
        return 0;
    }

    if (!tileIsDoor(mapTileAt(c->map, x, y)) &&
        !tileIsLockedDoor(mapTileAt(c->map, x, y)))
        return 0;

    if (tileIsLockedDoor(mapTileAt(c->map, x, y))) {
        screenMessage("Can't!\n");
        gameFinishTurn();
        return 1;
    }

    annotationSetTurnDuration(annotationAdd(x, y, BRICKFLOOR_TILE), 4);
    
    screenMessage("\nOpened!\n");
    gameFinishTurn();

    return 1;
}

/**
 * Waits for space bar to return from gem mode.
 */
int gemHandleChoice(char choice) {
    eventHandlerPopKeyHandler();

    viewMode = VIEW_NORMAL;
    gameFinishTurn();

    return 1;
}

/**
 * Handles the Exit (Y/N) choice.
 */
int quitHandleChoice(char choice) {
    FILE *saveGameFile, *monstersFile;

    eventHandlerPopKeyHandler();

    saveGameFile = fopen("party.sav", "wb");
    if (saveGameFile) {
        saveGameWrite(c->saveGame, saveGameFile);
        fclose(saveGameFile);
    } else {
        screenMessage("Error writing to\nparty.sav\n");
        choice = 'n';
    }

    monstersFile = fopen("monsters.sav", "wb");
    if (monstersFile) {
        saveGameMonstersWrite(c->map->objects, monstersFile);
        fclose(monstersFile);
    } else {
        screenMessage("Error writing to\nmonsters.sav\n");
        choice = 'n';
    }

    switch (choice) {
    case 'y':
        eventHandlerSetExitFlag(1);
        break;
    case 'n':
        screenMessage("%c\n", choice);
        gameFinishTurn();
        break;
    default:
        assert(0);              /* shouldn't happen */
    }

    return 1;
}

/**
 * Begins a conversation with the NPC at map coordinates x,y.  If no
 * NPC is present at that point, zero is returned.
 */
int talkAtCoord(int x, int y, int distance) {
    const Person *talker;
    char *text;

    if (x == -1 && y == -1) {
        screenMessage("Funny, no\nresponse!\n");
        gameFinishTurn();
        return 0;
    }

    c->conversation.talker = mapPersonAt(c->map, x, y);
    if (c->conversation.talker == NULL) {
        return 0;
    }

    talker = c->conversation.talker;
    c->conversation.state = CONV_INTRO;
    c->conversation.buffer[0] = '\0';
    
    personGetConversationText(&c->conversation, "", &text);
    screenMessage("\n\n%s", text);
    free(text);
    if (c->conversation.state == CONV_DONE)
        gameFinishTurn();
    else
        talkSetHandler(&c->conversation);

    return 1;
}

/**
 * Set up a key handler to handle the current conversation state.
 */
void talkSetHandler(const Conversation *cnv) {
    ReadBufferActionInfo *rbInfo;
    GetChoiceActionInfo *gcInfo;

    switch (personGetInputRequired(cnv)) {
    case CONVINPUT_STRING:
        rbInfo = (ReadBufferActionInfo *) malloc(sizeof(ReadBufferActionInfo));
        rbInfo->buffer = c->conversation.buffer;
        rbInfo->bufferLen = CONV_BUFFERLEN;
        rbInfo->handleBuffer = &talkHandleBuffer;
        rbInfo->screenX = TEXT_AREA_X + c->col;
        rbInfo->screenY = TEXT_AREA_Y + c->line;
        eventHandlerPushKeyHandlerData(&keyHandlerReadBuffer, rbInfo);
        break;
    
    case CONVINPUT_CHARACTER:
        gcInfo = (GetChoiceActionInfo *) malloc(sizeof(GetChoiceActionInfo));
        gcInfo->choices = personGetChoices(cnv);
        gcInfo->handleChoice = &talkHandleChoice;
        eventHandlerPushKeyHandlerData(&keyHandlerGetChoice, gcInfo);
        break;

    case CONVINPUT_NONE:
        /* no handler: conversation done! */
        break;
    }
}

/**
 * Handles a query while talking to an NPC.
 */
int talkHandleBuffer(const char *message) {
    char *reply, *prompt;

    eventHandlerPopKeyHandler();

    personGetConversationText(&c->conversation, message, &reply);
    screenMessage("%s", reply);
    free(reply);
        
    c->conversation.buffer[0] = '\0';

    if (c->conversation.state == CONV_DONE) {
        gameFinishTurn();
        return 1;
    }

    personGetPrompt(&c->conversation, &prompt);
    if (prompt) {
        screenMessage("%s", prompt);
        free(prompt);
    }

    talkSetHandler(&c->conversation);

    return 1;
}

int talkHandleChoice(char choice) {
    char message[2];
    char *reply, *prompt;

    eventHandlerPopKeyHandler();

    message[0] = choice;
    message[1] = '\0';

    personGetConversationText(&c->conversation, message, &reply);
    screenMessage("\n\n%s\n", reply);
    free(reply);
        
    c->conversation.buffer[0] = '\0';

    if (c->conversation.state == CONV_DONE) {
        gameFinishTurn();
        return 1;
    }

    personGetPrompt(&c->conversation, &prompt);
    if (prompt) {
        screenMessage("%s", prompt);
        free(prompt);
    }

    talkSetHandler(&c->conversation);

    return 1;
}

int useItem(const char *itemName) {
    eventHandlerPopKeyHandler();

    itemUse(itemName);

    gameFinishTurn();

    return 1;
}

/**
 * Changes armor for the given player.  Prompts the use for the armor.
 */
int wearForPlayer(int player) {
    AlphaActionInfo *info;

    c->statsItem = STATS_ARMOR;
    statsUpdate();

    info = (AlphaActionInfo *) malloc(sizeof(AlphaActionInfo));
    info->lastValidLetter = ARMR_MAX + 'a' - 1;
    info->handleAlpha = wearForPlayer2;
    info->prompt = "Armour: ";
    info->data = (void *) player;

    screenMessage("%s", info->prompt);

    eventHandlerPushKeyHandlerData(&gameGetAlphaChoiceKeyHandler, info);

    return 1;
}

int wearForPlayer2(int a, void *data) {
    int player = (int) data;
    ArmorType armor = (ArmorType) a, oldArmor;

    if (armor != ARMR_NONE && c->saveGame->armor[armor] < 1) {
        screenMessage("None left!\n");
        gameFinishTurn();
        return 0;
    }

    if (!playerCanWear(&(c->saveGame->players[player]), armor)) {
        screenMessage("\nA %s may NOT\nuse\n%s\n", getClassName(c->saveGame->players[player].klass), getArmorName(armor));
        gameFinishTurn();
        return 0;
    }

    oldArmor = c->saveGame->players[player].armor;
    if (oldArmor != ARMR_NONE)
        c->saveGame->armor[oldArmor]++;
    if (armor != ARMR_NONE)
        c->saveGame->armor[armor]--;
    c->saveGame->players[player].armor = armor;

    screenMessage("%s\n", getArmorName(armor));

    gameFinishTurn();

    return 1;
}

/**
 * Attempt to move the avatar in the given direction.  User event
 * should be set if the avatar is being moved in response to a
 * keystroke.  Returns zero if the avatar is blocked.
 */
int moveAvatar(Direction dir, int userEvent) {
    int result = 1;
    int newx, newy;

    /*musicPlayEffect();*/

    if (tileIsBalloon(c->saveGame->transport) && userEvent) {
        screenMessage("Drift Only!\n");
        goto done;
    }

    if (tileIsShip(c->saveGame->transport)) {
        if (tileGetDirection(c->saveGame->transport) != dir) {
            tileSetDirection(&c->saveGame->transport, dir);
            screenMessage("Turn %s!\n", getDirectionName(dir));
            goto done;
        }
    }

    if (tileIsHorse(c->saveGame->transport)) {
        if ((dir == DIR_WEST || dir == DIR_EAST) &&
            tileGetDirection(c->saveGame->transport) != dir) {
            tileSetDirection(&c->saveGame->transport, dir);
        }
    }

    newx = c->saveGame->x;
    newy = c->saveGame->y;
    dirMove(dir, &newx, &newy);

    if (tileIsShip(c->saveGame->transport))
        screenMessage("Sail %s!\n", getDirectionName(dir));
    else if (!tileIsBalloon(c->saveGame->transport))
        screenMessage("%s\n", getDirectionName(dir));

    if (MAP_IS_OOB(c->map, newx, newy)) {
	switch (c->map->border_behavior) {
	case BORDER_WRAP:
	    if (newx < 0)
		newx += c->map->width;
	    if (newy < 0)
		newy += c->map->height;
	    if (newx >= (int) c->map->width)
		newx -= c->map->width;
	    if (newy >= (int) c->map->height)
		newy -= c->map->height;
	    break;

	case BORDER_EXIT2PARENT:
	    if (c->parent != NULL) {
		Context *t = c;
                annotationClear();
                mapClearObjects(c->map);
                c->parent->saveGame->x = c->saveGame->dngx;
                c->parent->saveGame->y = c->saveGame->dngy;
                c->parent->line = c->line;
                c->parent->moonPhase = c->moonPhase;
                c->parent->windDirection = c->windDirection;
                c->parent->windCounter = c->windCounter;
                c->parent->aura = c->aura;
                c->parent->auraDuration = c->auraDuration;
                c->parent->horseSpeed = c->horseSpeed;
		c = c->parent;
                c->col = 0;
		free(t);
                
                screenMessage("Leaving...\n");
                musicPlay();
	    }
            goto done;
	    
	case BORDER_FIXED:
	    if (newx < 0 || newx >= (int) c->map->width)
		newx = c->saveGame->x;
	    if (newy < 0 || newy >= (int) c->map->height)
		newy = c->saveGame->y;
	    break;
	}
    }

    if (!collisionOverride) {
        int movementMask;
        int slow;

        movementMask = mapGetValidMoves(c->map, c->saveGame->x, c->saveGame->y, c->saveGame->transport);
        if (!DIR_IN_MASK(dir, movementMask)) {

            if (settings->shortcutCommands) {
                if (tileIsDoor(mapTileAt(c->map, newx, newy))) {
                    openAtCoord(newx, newy, 1);
                    goto done;
                } else if (tileIsLockedDoor(mapTileAt(c->map, newx, newy))) {
                    jimmyAtCoord(newx, newy, 1);
                    goto done;
                }
            }

            screenMessage("Blocked!\n");
            result = 0;
            goto done;
        }

        switch (tileGetSpeed(mapTileAt(c->map, newx, newy))) {
        case FAST:
            slow = 0;
            break;
        case SLOW:
            slow = (rand() % 8) == 0;
            break;
        case VSLOW:
            slow = (rand() % 4) == 0;
            break;
        case VVSLOW:
            slow = (rand() % 2) == 0;
            break;
        }

        if (tileIsShip(c->saveGame->transport) && gameWindSlowsShip(dir))
            slow = 1;

        if (slow) {
            screenMessage("Slow progress!\n");
            result = 0;
            goto done;
        }
    }

    c->saveGame->x = newx;
    c->saveGame->y = newy;

    gameCheckBridgeTrolls();
    gameCheckSpecialMonsters(dir);
    gameCheckMoongates();

 done:

    return result;
}

/**
 * This function is called every quarter second.
 */
void gameTimer(void *data) {
    int oldTrammel, trammelSubphase;
    const Moongate *gate;

    Direction dir = DIR_WEST;
    if (++c->windCounter >= MOON_SECONDS_PER_PHASE * 4) {
        if ((rand() % 4) == 1)
            c->windDirection = dirRandomDir(MASK_DIR_ALL);
        c->windCounter = 0;
        if (tileIsBalloon(c->saveGame->transport) &&
            c->saveGame->balloonstate) {
            dir = dirReverse((Direction) c->windDirection);
            moveAvatar(dir, 0);
        }
    }

    /* update moon phases */
    if (mapIsWorldMap(c->map) && viewMode == VIEW_NORMAL) {
        oldTrammel = c->saveGame->trammelphase;

        if (++c->moonPhase >= (MOON_SECONDS_PER_PHASE * 4 * MOON_PHASES))
            c->moonPhase = 0;

        c->saveGame->trammelphase = c->moonPhase / (MOON_SECONDS_PER_PHASE * 4) / 3;
        c->saveGame->feluccaphase = c->moonPhase / (MOON_SECONDS_PER_PHASE * 4) % 8;

        if (--c->saveGame->trammelphase > 7)
            c->saveGame->trammelphase = 7;
        if (--c->saveGame->feluccaphase > 7)
            c->saveGame->feluccaphase = 7;

        trammelSubphase = c->moonPhase % (MOON_SECONDS_PER_PHASE * 4 * 3);

        /* update the moongates if trammel changed */
        if (trammelSubphase == 0) {
            gate = moongateGetGateForPhase(oldTrammel);
            annotationRemove(gate->x, gate->y, MOONGATE0_TILE);
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE0_TILE));
        }
        else if (trammelSubphase == 1) {
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationRemove(gate->x, gate->y, MOONGATE0_TILE);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE1_TILE));
        }
        else if (trammelSubphase == 2) {
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationRemove(gate->x, gate->y, MOONGATE1_TILE);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE2_TILE));
        }
        else if (trammelSubphase == 3) {
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationRemove(gate->x, gate->y, MOONGATE2_TILE);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE3_TILE));
        }
        else if (trammelSubphase == (MOON_SECONDS_PER_PHASE * 4 * 3) - 3) {
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationRemove(gate->x, gate->y, MOONGATE3_TILE);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE2_TILE));
        }
        else if (trammelSubphase == (MOON_SECONDS_PER_PHASE * 4 * 3) - 2) {
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationRemove(gate->x, gate->y, MOONGATE2_TILE);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE1_TILE));
        }
        else if (trammelSubphase == (MOON_SECONDS_PER_PHASE * 4 * 3) - 1) {
            gate = moongateGetGateForPhase(c->saveGame->trammelphase);
            annotationRemove(gate->x, gate->y, MOONGATE1_TILE);
            annotationSetVisual(annotationAdd(gate->x, gate->y, MOONGATE0_TILE));
        }
    }

    mapAnimateObjects(c->map);

    screenCycle();

    /* 
     * refresh the screen only if the timer queue is empty --
     * i.e. drop a frame if another timer event is about to be fired
     */
    if (eventHandlerTimerQueueEmpty())
        gameUpdateScreen();

    /*
     * force pass if no commands within last 20 seconds
     */
    if (eventHandlerGetKeyHandler() == &gameBaseKeyHandler &&
        gameTimeSinceLastCommand() > 20)
        gameBaseKeyHandler(' ', NULL);
        
}

void gameCheckBridgeTrolls() {
    if (!mapIsWorldMap(c->map) ||
        mapTileAt(c->map, c->saveGame->x, c->saveGame->y) != BRIDGE_TILE ||
        (rand() % 8) != 0)
        return;

    screenMessage("\nBridge Trolls!\n");

    combatBegin(mapTileAt(c->map, c->saveGame->x, c->saveGame->y), c->saveGame->transport, 
                mapAddObject(c->map, TROLL_TILE, TROLL_TILE, c->saveGame->x, c->saveGame->y));
}

void gameCheckSpecialMonsters(Direction dir) {
    int i;
    Object *obj;
    static const struct {
        int x, y;
        Direction dir;
    } pirateInfo[] = {
        { 224, 220, DIR_EAST }, /* N'M" O'A" */
        { 224, 228, DIR_EAST }, /* O'E" O'A" */
        { 226, 220, DIR_EAST }, /* O'E" O'C" */
        { 227, 228, DIR_EAST }, /* O'E" O'D" */
        { 228, 227, DIR_SOUTH }, /* O'D" O'E" */
        { 229, 225, DIR_SOUTH }, /* O'B" O'F" */
        { 229, 223, DIR_NORTH }, /* N'P" O'F" */
        { 228, 222, DIR_NORTH } /* N'O" O'E" */
    };

    if (dir == DIR_EAST &&
        c->saveGame->x == 0xdd &&
        c->saveGame->y == 0xe0) {
        for (i = 0; i < 8; i++) {
            unsigned short tile = PIRATE_TILE;
            tileSetDirection(&tile, pirateInfo[i].dir);
            obj = mapAddObject(c->map, tile, tile, pirateInfo[i].x, pirateInfo[i].y);
            obj->movement_behavior = MOVEMENT_ATTACK_AVATAR;
        }
    }

    if (dir == DIR_SOUTH &&
        c->saveGame->x >= 229 &&
        c->saveGame->x < 234 &&
        c->saveGame->y >= 212 &&
        c->saveGame->y < 217) {
        for (i = 0; i < 8; i++) {
            obj = mapAddObject(c->map, DAEMON_TILE, DAEMON_TILE, 231, c->saveGame->y + 1);
            obj->movement_behavior = MOVEMENT_ATTACK_AVATAR;
        }
    }
}


void gameCheckMoongates() {
    int destx, desty;
    extern Map shrine_spirituality_map;
    
    if (moongateFindActiveGateAt(c->saveGame->trammelphase, c->saveGame->feluccaphase, 
                                 c->saveGame->x, c->saveGame->y, &destx, &desty)) {
        
        /* FIXME: special effect here */
        c->saveGame->x = destx;
        c->saveGame->y = desty;

        if (moongateIsEntryToShrineOfSpirituality(c->saveGame->trammelphase, c->saveGame->feluccaphase)) {
            if (!playerCanEnterShrine(c->saveGame, shrine_spirituality_map.shrine->virtue))
                return;
            else
                shrineEnter(shrine_spirituality_map.shrine);

            annotationClear();  /* clear out world map annotations */

            c = gameCloneContext(c);

            gameSetMap(c, &shrine_spirituality_map, 1);
            musicPlay();
        }
    }
}

void gameCheckRandomMonsters() {
    int x, y, dx, dy, t;
    const Monster *monster;
    Object *obj;

    if (!mapIsWorldMap(c->map) ||
        mapNumberOfMonsters(c->map) >= 4 ||
        (rand() % 16) != 0)
        return;

    dx = 7;
    dy = rand() % 7;

    if (rand() % 2)
        dx = -dx;
    if (rand() % 2)
        dy = -dy;
    if (rand() % 2) {
        t = dx;
        dx = dy;
        dy = t;
    }

    x = c->saveGame->x + dx;
    y = c->saveGame->y + dy;

    if ((monster = monsterRandomForTile(mapTileAt(c->map, x, y))) == 0)
        return;

    obj = mapAddObject(c->map, monster->tile, monster->tile, x, y);
    if (monster->mattr & MATTR_NONATTACKABLE)
        obj->movement_behavior = MOVEMENT_WANDER;
    else
        obj->movement_behavior = MOVEMENT_ATTACK_AVATAR;
}

long gameTimeSinceLastCommand() {
    return time(NULL) - c->lastCommandTime;
}

/**
 * Check whether a ship movement in a given direction is slowed down
 * by the wind.
 */
int gameWindSlowsShip(Direction shipdir) {
    if (shipdir == (Direction) c->windDirection)
        return (c->saveGame->moves % 4) != 3;
    else if (shipdir == dirReverse((Direction) c->windDirection))
        return (c->saveGame->moves % 4) == 0;
    else
        return 0;
}
