/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "u4.h"
#include "screen.h"
#include "event.h"
#include "map.h"
#include "person.h"
#include "ttype.h"
#include "context.h"
#include "annotation.h"
#include "savegame.h"
#include "stats.h"
#include "spell.h"

void moveAvatar(int dx, int dy);
int attackAtCoord(int x, int y);
int castForPlayer(int player);
int castForPlayer2(int spell, void *data);
int jimmyAtCoord(int x, int y);
int newOrderForPlayer(int player);
int newOrderForPlayer2(int player2);
int openAtCoord(int x, int y);
int readyForPlayer(int player);
int readyForPlayer2(int weapon, void *data);
int talkAtCoord(int x, int y);
int talkHandleBuffer(const char *message);
int wearForPlayer(int player);
int wearForPlayer2(int armor, void *data);

typedef struct TimerCallbackNode {
    void (*callback)();
    struct TimerCallbackNode *next;
} TimerCallbackNode;

TimerCallbackNode *timerCallbackHead = NULL;
KeyHandlerNode *keyHandlerHead = NULL;
int eventExitFlag = 0;

/**
 * Set flag to end eventHandlerMain loop.
 */
void eventHandlerSetExitFlag(int flag) {
    eventExitFlag = flag;
}

/**
 * Get flag that controls eventHandlerMain loop.
 */
int eventHandlerGetExitFlag() {
    return eventExitFlag;
}

void eventHandlerAddTimerCallback(void (*callback)()) {
    TimerCallbackNode *n = malloc(sizeof(TimerCallbackNode));
    if (n) {
        n->callback = callback;
        n->next = timerCallbackHead;
        timerCallbackHead = n;
    }
}

/**
 * Trigger all the timer callbacks.
 */
void eventHandlerCallTimerCallbacks() {
    TimerCallbackNode *n;    

    for (n = timerCallbackHead; n != NULL; n = n->next) {
        (*timerCallbackHead->callback)();
    }
}

/**
 * This function is called every quarter second.
 */
void eventTimer() {
    screenAnimate();

    if (++c->moonPhase >= (MOON_SECONDS_PER_PHASE * 4 * MOON_PHASES))
        c->moonPhase = 0;
}


/**
 * Push a key handler onto the top of the keyhandler stack.
 */
void eventHandlerPushKeyHandler(KeyHandler kh) {
    KeyHandlerNode *n = malloc(sizeof(KeyHandlerNode));
    if (n) {
        n->kh = kh;
        n->data = NULL;
        n->next = keyHandlerHead;
        keyHandlerHead = n;
    }
}

/**
 * Push a key handler onto the top of the key handler stack, and
 * provide specific data to pass to the handler.
 */
void eventHandlerPushKeyHandlerData(KeyHandler kh, void *data) {
    KeyHandlerNode *n = malloc(sizeof(KeyHandlerNode));
    if (n) {
        n->kh = kh;
        n->data = data;
        n->next = keyHandlerHead;
        keyHandlerHead = n;
    }
}

/**
 * Pop the top key handler off.
 */
void eventHandlerPopKeyHandler() {
    KeyHandlerNode *n = keyHandlerHead;
    if (n) {
        keyHandlerHead = n->next;
        free(n);
    }
}

/**
 * Get the currently active key handler of the top of the key handler
 * stack.
 */
KeyHandler eventHandlerGetKeyHandler() {
    return keyHandlerHead->kh;
}

/**
 * Get the call data associated with the currently active key handler.
 */
void *eventHandlerGetKeyHandlerData() {
    return keyHandlerHead->data;
}

int keyHandlerDefault(int key, void *data) {
    int valid = 1;

    switch (key) {
    case '`':
        printf("x = %d, y = %d, tile = %d\n", c->saveGame->x, c->saveGame->y, mapTileAt(c->map, c->saveGame->x, c->saveGame->y));
        break;
    case 'm':
        screenMessage("0123456789012345\n");
        break;
    default:
        valid = 0;
        break;
    }

    return valid;
}

int keyHandlerNormal(int key, void *data) {
    int valid = 1;
    const Portal *portal;
    DirectedActionInfo *info;

    switch (key) {

    case U4_UP:
        moveAvatar(0, -1);
        break;

    case U4_DOWN:
        moveAvatar(0, 1);
        break;

    case U4_LEFT:
        moveAvatar(-1, 0);
        break;

    case U4_RIGHT:
        moveAvatar(1, 0);
        break;

    case 'a':
        info = (DirectedActionInfo *) malloc(sizeof(DirectedActionInfo));
        info->handleAtCoord = &attackAtCoord;
        info->range = 1;
        info->blockedPredicate = NULL;
        info->failedMessage = "FIXME";
        eventHandlerPushKeyHandlerData(&keyHandlerGetDirection, info);
        screenMessage("Attack\nDir: ");
        break;

    case 'c':
        eventHandlerPushKeyHandlerData(&keyHandlerGetPlayerNo, &castForPlayer);
        screenMessage("Cast Spell!\nPlayer: ");
        break;

    case 'd':
        portal = mapPortalAt(c->map, c->saveGame->x, c->saveGame->y);
        if (portal && portal->trigger_action == ACTION_DESCEND) {
            c->map = portal->destination;
            screenMessage("Descend!\n\n\020");
        }
        else
            screenMessage("Descend what?\n\020");
        break;

    case 'e':
        portal = mapPortalAt(c->map, c->saveGame->x, c->saveGame->y);
        if (portal && portal->trigger_action == ACTION_ENTER) {

            Context *new = (Context *) malloc(sizeof(Context));
            new->parent = c;
            new->saveGame = c->saveGame;
            new->map = portal->destination;
            new->annotation = NULL;
            new->saveGame->dngx = new->parent->saveGame->x;
            new->saveGame->dngy = new->parent->saveGame->y;
            new->saveGame->x = new->map->startx;
            new->saveGame->y = new->map->starty;
            new->col = new->parent->col;
            new->line = new->parent->line;
            new->statsItem = new->parent->statsItem;
            new->moonPhase = new->parent->moonPhase;
            c = new;

            screenMessage("Enter towne!\n\n%s\n\n\020", c->map->name);
        } else
            screenMessage("Enter what?\n\020");
        break;

    case 'j':
        info = (DirectedActionInfo *) malloc(sizeof(DirectedActionInfo));
        info->handleAtCoord = &jimmyAtCoord;
        info->range = 1;
        info->blockedPredicate = NULL;
        info->failedMessage = "Jimmy what?";
        eventHandlerPushKeyHandlerData(&keyHandlerGetDirection, info);
        screenMessage("Jimmy\nDir: ");
        break;
        
    case 'k':
        portal = mapPortalAt(c->map, c->saveGame->x, c->saveGame->y);
        if (portal && portal->trigger_action == ACTION_KLIMB) {
            c->map = portal->destination;
            screenMessage("Klimb!\n\n\020");
        }
        else
            screenMessage("Klimb what?\n\020");
        break;

    case 'l':
        if (c->saveGame->sextants >= 1)
            screenMessage("Locate position\nwith sextant\n Latitude: %c'%c\"\nLongitude: %c'%c\"\n\020",
                          c->saveGame->y / 16 + 'A', c->saveGame->y % 16 + 'A',
                          c->saveGame->x / 16 + 'A', c->saveGame->x % 16 + 'A');
        else
            screenMessage("Locate position\nwith what?\n\020");
        break;

    case 'n':
        eventHandlerPushKeyHandlerData(&keyHandlerGetPlayerNo, &newOrderForPlayer);
        screenMessage("New Order!\nExchange # ");
        break;

    case 'o':
        info = (DirectedActionInfo *) malloc(sizeof(DirectedActionInfo));
        info->handleAtCoord = &openAtCoord;
        info->range = 1;
        info->blockedPredicate = NULL;
        info->failedMessage = "Open what?";
        eventHandlerPushKeyHandlerData(&keyHandlerGetDirection, info);
        screenMessage("Open\nDir: ");
        break;

    case 'q':
        if (strcmp(c->map->name, "World") != 0) {
            screenMessage("Quit & save\nNot Here!\n");
        } else {
            eventHandlerPushKeyHandler(&keyHandlerQuit);
            screenMessage("Quit & save\nExit (Y/N)? ");
        }
        break;

    case 'r':
        eventHandlerPushKeyHandlerData(&keyHandlerGetPlayerNo, &readyForPlayer);
        screenMessage("Ready a weapon\nfor: ");
        break;

    case 't':
        info = (DirectedActionInfo *) malloc(sizeof(DirectedActionInfo));
        info->handleAtCoord = &talkAtCoord;
        info->range = 2;
        info->blockedPredicate = &tileCanTalkOver;
        info->failedMessage = "Funny, no\nresponse!";
        eventHandlerPushKeyHandlerData(&keyHandlerGetDirection, info);
        screenMessage("Talk\nDir: ");
        break;

    case 'w':
        eventHandlerPushKeyHandlerData(&keyHandlerGetPlayerNo, &wearForPlayer);
        screenMessage("Wear Armor\nfor: ");
        break;

    case 'y':
        screenMessage("Yell what?\n");
        break;

    case 'z':
        eventHandlerPushKeyHandler(&keyHandlerZtats);
        screenMessage("Ztats for: ");
        break;

    default:
        valid = 0;
        break;
    }

    if (valid)
        annotationCycle();

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses when a command that requires a player number
 * argument is invoked.  Once a number key is pressed, control is
 * handed off to a command specific routine.
 */
int keyHandlerGetPlayerNo(int key, void *data) {
    int (*handlePlayerNo)(int player) = (int(*)(int))data;
    int valid = 1;

    eventHandlerPopKeyHandler();

    if (key >= '1' &&
        key <= ('0' + c->saveGame->members)) {
        screenMessage("%c\n", key);
        handlePlayerNo(key - '1');
    } else {
        screenMessage("None\n");
        valid = 0;
    }

    return valid || keyHandlerDefault(key, NULL);
}

int keyHandlerGetAlphaChoice(int key, void *data) {
    AlphaActionInfo *info = (AlphaActionInfo *) data;
    int valid = 1;


    if (isupper(key))
        key = tolower(key);

    if (key >= 'a' && key <= info->lastValidLetter) {
        screenMessage("%c\n", key);
        (*(info->handleAlpha))(key - 'a', info->data);
        eventHandlerPopKeyHandler();
        free(info);
        c->statsItem = STATS_PARTY_OVERVIEW;
        statsUpdate();
    } else if (key == ' ' || key == U4_ESC) {
        eventHandlerPopKeyHandler();
        free(info);
        screenMessage("\n\020");
        c->statsItem = STATS_PARTY_OVERVIEW;
        statsUpdate();
    } else {
        valid = 0;
        screenMessage("\n%s", info->prompt);
        screenForceRedraw();
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses when a command that requires a direction
 * argument is invoked.  Once an arrow key is pressed, control is
 * handed off to a command specific routine.
 */
int keyHandlerGetDirection(int key, void *data) {
    DirectedActionInfo *info = (DirectedActionInfo *) data;
    int valid = 1;
    int i;
    int t_x = c->saveGame->x, t_y = c->saveGame->y;

    eventHandlerPopKeyHandler();

    switch (key) {
    case U4_UP:
        screenMessage("North\n");
        break;
    case U4_DOWN:
        screenMessage("South\n");
        break;
    case U4_LEFT:
        screenMessage("West\n");
        break;
    case U4_RIGHT:
        screenMessage("East\n");
        break;
    }

    /* 
     * try every tile in the given direction, up to the given range.
     * Stop when the command handler succeeds, the range is exceeded,
     * or the action is blocked.
     */
    for (i = 1; i <= info->range; i++) {
        switch (key) {
        case U4_UP:
            t_y--;
            break;
        case U4_DOWN:
            t_y++;
            break;
        case U4_LEFT:
            t_x--;
            break;
        case U4_RIGHT:
            t_x++;
            break;
        default:
            t_x = -1;
            t_y = -1;
            valid = 0;
            break;
        }

        if ((*(info->handleAtCoord))(t_x, t_y))
            goto success;
        if (info->blockedPredicate &&
            !(*(info->blockedPredicate))(mapTileAt(c->map, t_x, t_y)))
            break;
    }

    screenMessage("%s\n", info->failedMessage);
    c->statsItem = STATS_PARTY_OVERVIEW;
    statsUpdate();

 success:
    free(info);

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses when a buffer is being read, such as when a
 * conversation is active.  The keystrokes are buffered up into a word
 * rather than invoking the usual commands.
 */
int keyHandlerReadBuffer(int key, void *data) {
    ReadBufferActionInfo *info = (ReadBufferActionInfo *) data;
    int valid = 1;

    if ((key >= 'a' && key <= 'z') || 
        (key >= 'A' && key <= 'Z') ||
        key == ' ') {
        int len;

        len = strlen(info->buffer);
        if (len < info->bufferLen - 1) {
            screenDisableCursor();
            screenTextAt(info->screenX + len, info->screenY, "%c", key);
            screenSetCursorPos(info->screenX + len + 1, info->screenY);
            screenEnableCursor();
            info->buffer[len] = key;
            info->buffer[len + 1] = '\0';
        }

    } else if (key == U4_BACKSPACE) {
        int len;


        len = strlen(info->buffer);
        if (len > 0) {
            screenDisableCursor();
            screenTextAt(info->screenX + len - 1, info->screenY, " ");
            screenSetCursorPos(info->screenX + len - 1, info->screenY);
            screenEnableCursor();
            info->buffer[len - 1] = '\0';
        }

    } else if (key == '\n' || key == '\r') {
        if ((*info->handleBuffer)(info->buffer))
            free(info);
        else
            info->screenX -= strlen(info->buffer);

    } else {
        valid = 0;
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses when the Exit (Y/N) prompt is active.
 */
int keyHandlerQuit(int key, void *data) {
    FILE *saveGameFile;
    int valid = 1;
    char answer = '?';

    switch (key) {
    case 'y':
    case 'n':
        answer = key;
        break;
    default:
        valid = 0;
        break;
    }

    if (answer == 'y' || answer == 'n') {
        saveGameFile = fopen("party.sav", "wb");
        if (saveGameFile) {
            saveGameWrite(c->saveGame, saveGameFile);
            fclose(saveGameFile);
        } else {
            screenMessage("Error writing to\nparty.sav\n");
            answer = '?';
        }

        if (answer == 'y')
            exit(0);
        else if (answer == 'n') {
            screenMessage("%c\n", key);
            eventHandlerPopKeyHandler();            
        }
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses when the Ztats prompt is active.
 */
int keyHandlerZtats(int key, void *data) {
    int valid = 1;

    screenMessage("%c\n", key);

    if (key == '0')
        c->statsItem = STATS_WEAPONS;
    else if (key >= '1' && key <= '8' && (key - '1' + 1) <= c->saveGame->members)
        c->statsItem = STATS_CHAR1 + (key - '1');
    else
        valid = 0;

    if (valid) {
        statsUpdate();
        eventHandlerPopKeyHandler();
        eventHandlerPushKeyHandler(&keyHandlerZtats2);
    }

    return valid || keyHandlerDefault(key, NULL);
}

/**
 * Handles key presses while Ztats are being displayed.
 */
int keyHandlerZtats2(int key, void *data) {
    switch (key) {
    case U4_UP:
    case U4_LEFT:
        c->statsItem--;
        if (c->statsItem < STATS_CHAR1)
            c->statsItem = STATS_MIXTURES;
        if (c->statsItem <= STATS_CHAR8 &&
            (c->statsItem - STATS_CHAR1 + 1) > c->saveGame->members)
            c->statsItem = STATS_CHAR1 - 1 + c->saveGame->members;
        break;
    case U4_DOWN:
    case U4_RIGHT:
        c->statsItem++;
        if (c->statsItem > STATS_MIXTURES)
            c->statsItem = STATS_CHAR1;
        if (c->statsItem <= STATS_CHAR8 &&
            (c->statsItem - STATS_CHAR1 + 1) > c->saveGame->members)
            c->statsItem = STATS_WEAPONS;
        break;
    default:
        c->statsItem = STATS_PARTY_OVERVIEW;
        eventHandlerPopKeyHandler();
        screenMessage("\020");
        break;
    }

    statsUpdate();

    return 1;
}

/**
 * Attempts to attack a creature at map coordinates x,y.  If no
 * creature is present at that point, zero is returned.
 */
int attackAtCoord(int x, int y) {
    if (x == -1 && y == -1)
        return 0;

    screenMessage("attack at %d, %d\n(not implemented)\n", x, y);

    return 1;
}

int castForPlayer(int player) {
    AlphaActionInfo *info;

    c->statsItem = STATS_MIXTURES;
    statsUpdate();

    info = (AlphaActionInfo *) malloc(sizeof(AlphaActionInfo));
    info->lastValidLetter = 'z';
    info->handleAlpha = castForPlayer2;
    info->prompt = "Spell: ";
    info->data = (void *) player;

    screenMessage("%s", info->prompt);

    eventHandlerPushKeyHandlerData(&keyHandlerGetAlphaChoice, info);

    return 1;
}

int castForPlayer2(int spell, void *data) {
    int player = (int) data;
    SpellCastError spellError;

    if (!spellCast(spell, player, &spellError)) {
        switch(spellError) {
        case CASTERR_NOMIX:
            screenMessage("None Mixed!\n");
            break;
        case CASTERR_WRONGCONTEXT:
            screenMessage("Can't Cast Here!\n");
            break;
        case CASTERR_MPTOOLOW:
            screenMessage("Not Enough MP!\n");
            break;
        case CASTERR_NOERROR:
        default:
            /* should never happen */
            assert(0);
        }
    }
    screenMessage("\n\020");

    return 1;
}

/**
 * Attempts to jimmy a locked door at map coordinates x,y.  If no
 * locked door is present at that point, zero is returned.  The locked
 * door is replaced by a permanent annotation of an unlocked door
 * tile.
 */
int jimmyAtCoord(int x, int y) {
    if ((x == -1 && y == -1) ||
        !islockeddoor(mapTileAt(c->map, x, y)))
        return 0;


    screenMessage("door at %d, %d unlocked!\n", x, y);
    annotationAdd(x, y, -1, 0x3b);

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

    eventHandlerPushKeyHandlerData(&keyHandlerGetAlphaChoice, info);

    return 1;
}

int readyForPlayer2(int weapon, void *data) {
    int player = (int) data;
    int oldWeapon;

    if (weapon != WEAP_HANDS && c->saveGame->weapons[weapon] < 1) {
        screenMessage("None left!\n\020");
        return 0;
    }

    oldWeapon = c->saveGame->players[player].weapon;
    if (oldWeapon != WEAP_HANDS)
        c->saveGame->weapons[oldWeapon]++;
    if (weapon != WEAP_HANDS)
        c->saveGame->weapons[weapon]--;
    c->saveGame->players[player].weapon = weapon;

    screenMessage("player %d, weapon %c\n\020", player, weapon + 'a');

    return 1;
}

/* FIXME: must be a better way.. */
int newOrderTemp;

/**
 * Exchanges the position of two players in the party.  Prompts the
 * use for the second player number.
 */
int newOrderForPlayer(int player) {
    if (player == 0) {
        screenMessage("%s, You must\nlead!\n\020", c->saveGame->players[0].name);
        return 0;
    }

    screenMessage("    with # ");

    newOrderTemp = player;
    eventHandlerPushKeyHandlerData(&keyHandlerGetPlayerNo, &newOrderForPlayer2);

    return 1;
}

int newOrderForPlayer2(int player2) {
    int player1 = newOrderTemp;
    SaveGamePlayerRecord tmp;

    if (player2 == 0) {
        screenMessage("%s, You must\nlead!\n\020", c->saveGame->players[0].name);
        return 0;
    } else if (player1 == player2) {
        screenMessage("\020");
        return 0;
    }

    tmp = c->saveGame->players[player1];
    c->saveGame->players[player1] = c->saveGame->players[player2];
    c->saveGame->players[player2] = tmp;

    statsUpdate();

    return 1;
}

/**
 * Attempts to open a door at map coordinates x,y.  If no door is
 * present at that point, zero is returned.  The door is replaced by a
 * temporary annotation of a floor tile for 4 turns.
 */
int openAtCoord(int x, int y) {
    if ((x == -1 && y == -1) ||
        !isdoor(mapTileAt(c->map, x, y)))
        return 0;

    screenMessage("door at %d, %d, opened!\n", x, y);
    annotationAdd(x, y, 4, 0x3e);

    return 1;
}

/**
 * Begins a conversation with the NPC at map coordinates x,y.  If no
 * NPC is present at that point, zero is returned.
 */
int talkAtCoord(int x, int y) {
    const Person *talker;
    char *intro;
    ReadBufferActionInfo *info;

    if (x == -1 && y == -1)
        return 0;

    c->conversation.talker = mapPersonAt(c->map, x, y);
    if (c->conversation.talker == NULL)
        return 0;

    talker = c->conversation.talker;
    c->conversation.question = 0;
    c->conversation.buffer[0] = '\0';
    
    personGetIntroduction(c->conversation.talker, &intro);
    screenMessage("\n\n%s\n", intro);
    free(intro);

    info = (ReadBufferActionInfo *) malloc(sizeof(ReadBufferActionInfo));
    info->buffer = c->conversation.buffer;
    info->bufferLen = CONV_BUFFERLEN;
    info->handleBuffer = &talkHandleBuffer;
    info->screenX = TEXT_AREA_X + c->col;
    info->screenY = TEXT_AREA_Y + c->line;

    eventHandlerPushKeyHandlerData(&keyHandlerReadBuffer, info);

    return 1;
}

/**
 * Handles a query while talking to an NPC.
 */
int talkHandleBuffer(const char *message) {
    int done;
    char *reply, *prompt;
    int askq;

    screenMessage("\n");

    if (c->conversation.question)
        done = personGetQuestionResponse(c->conversation.talker, message, &reply, &askq);
    else
        done = personGetResponse(c->conversation.talker, message, &reply, &askq);

    screenMessage("\n%s\n", reply);
    free(reply);
        
    c->conversation.question = askq;
    c->conversation.buffer[0] = '\0';

    if (done) {
        eventHandlerPopKeyHandler();
        screenMessage("\020");
        return 1;
    } else if (askq) {
        personGetQuestion(c->conversation.talker, &prompt);
        screenMessage("%s", prompt);
        free(prompt);
    } else {
        personGetPrompt(c->conversation.talker, &prompt);
        screenMessage("\n%s\n", prompt);
        free(prompt);
    }

    return 0;
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

    eventHandlerPushKeyHandlerData(&keyHandlerGetAlphaChoice, info);

    return 1;
}

int wearForPlayer2(int armor, void *data) {
    int player = (int) data;
    int oldArmor;

    if (armor != ARMR_NONE && c->saveGame->armor[armor] < 1) {
        screenMessage("None left!\n\020");
        return 0;
    }

    oldArmor = c->saveGame->players[player].armor;
    if (oldArmor != ARMR_NONE)
        c->saveGame->armor[oldArmor]++;
    if (armor != ARMR_NONE)
        c->saveGame->armor[armor]--;
    c->saveGame->players[player].armor = armor;

    screenMessage("player %d, armor %c\n\020", player, armor + 'a');

    return 1;
}

/**
 * Attempt to move the avatar.  The number of tiles to move is given
 * by dx (horizontally) and dy (vertically); negative indicates
 * right/down, positive indicates left/up.
 */
void moveAvatar(int dx, int dy) {
    int newx, newy;

    newx = c->saveGame->x + dx;
    newy = c->saveGame->y + dy;

    if (MAP_IS_OOB(c->map, newx, newy)) {
	switch (c->map->border_behavior) {
	case BORDER_WRAP:
	    if (newx < 0)
		newx += c->map->width;
	    if (newy < 0)
		newy += c->map->height;
	    if (newx >= c->map->width)
		newx -= c->map->width;
	    if (newy >= c->map->height)
		newy -= c->map->height;
	    break;

	case BORDER_EXIT2PARENT:
	    if (c->parent != NULL) {
		Context *t = c;
                c->parent->saveGame->x = c->saveGame->dngx;
                c->parent->saveGame->y = c->saveGame->dngy;
                c->parent->line = c->line;
                c->parent->moonPhase = c->moonPhase;
		c = c->parent;
		free(t);
	    }
	    return;
	    
	case BORDER_FIXED:
	    if (newx < 0 || newx >= c->map->width)
		newx = c->saveGame->x;
	    if (newy < 0 || newy >= c->map->height)
		newy = c->saveGame->y;
	    break;
	}
    }

    if (iswalkable(mapTileAt(c->map, newx, newy)) &&
          !mapPersonAt(c->map, newx, newy)) {
	c->saveGame->x = newx;
	c->saveGame->y = newy;
    }
}
