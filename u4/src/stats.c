/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "u4.h"

#include "stats.h"

#include "armor.h"
#include "context.h"
#include "debug.h"
#include "menu.h"
#include "names.h"
#include "player.h"
#include "savegame.h"
#include "screen.h"
#include "tile.h"
#include "weapon.h"

void statsAreaSetTitle(const char *title);
void statsShowCharDetails(int charNo);
void statsShowWeapons();
void statsShowArmor();
void statsShowEquipment();
void statsShowItems();
void statsShowReagents();
void statsShowMixtures();

/**
 * Sets the stats item to the previous in sequence.
 */
void statsPrevItem() {
    c->statsItem = (StatsItem) (c->statsItem - 1);
    if (c->statsItem < STATS_CHAR1)
        c->statsItem = STATS_MIXTURES;
    if (c->statsItem <= STATS_CHAR8 &&
        (c->statsItem - STATS_CHAR1 + 1) > c->saveGame->members)
        c->statsItem = (StatsItem) (STATS_CHAR1 - 1 + c->saveGame->members);
}

/**
 * Sets the stats item to the next in sequence.
 */
void statsNextItem() {
    c->statsItem = (StatsItem) (c->statsItem + 1);
    if (c->statsItem > STATS_MIXTURES)
        c->statsItem = STATS_CHAR1;
    if (c->statsItem <= STATS_CHAR8 &&
            (c->statsItem - STATS_CHAR1 + 1) > c->saveGame->members)
            c->statsItem = STATS_WEAPONS;
}

/**
 * Update the stats (ztats) box on the upper right of the screen.
 */
void statsUpdate() {
    int i;
    unsigned char mask;

    statsAreaClear();

    /*
     * update the upper stats box
     */
    switch(c->statsItem) {
    case STATS_PARTY_OVERVIEW:
        statsShowPartyView(-1);
        break;
    case STATS_CHAR1:
    case STATS_CHAR2:
    case STATS_CHAR3:
    case STATS_CHAR4:
    case STATS_CHAR5:
    case STATS_CHAR6:
    case STATS_CHAR7:
    case STATS_CHAR8:
        statsShowCharDetails(c->statsItem - STATS_CHAR1);
        break;
    case STATS_WEAPONS:
        statsShowWeapons();
        break;
    case STATS_ARMOR:
        statsShowArmor();
        break;
    case STATS_EQUIPMENT:
        statsShowEquipment();
        break;
    case STATS_ITEMS:
        statsShowItems();
        break;
    case STATS_REAGENTS:
        statsShowReagents();
        break;
    case STATS_MIXTURES:
        statsShowMixtures();
        break;
    }

    /*
     * update the lower stats box (food, gold, etc.)
     */
    if (c->transportContext == TRANSPORT_SHIP)
        screenTextAt(STATS_AREA_X, STATS_AREA_Y+STATS_AREA_HEIGHT+1, "F:%04d   SHP:%02d", c->saveGame->food / 100, c->saveGame->shiphull);
    else
        screenTextAt(STATS_AREA_X, STATS_AREA_Y+STATS_AREA_HEIGHT+1, "F:%04d   G:%04d", c->saveGame->food / 100, c->saveGame->gold);

    mask = 0xff;
    for (i = 0; i < VIRT_MAX; i++) {
        if (c->saveGame->karma[i] == 0)
            mask &= ~(1 << i);
    }

    switch (c->aura) {
    case AURA_NONE:
        screenShowCharMasked(0, STATS_AREA_X + STATS_AREA_WIDTH/2, STATS_AREA_Y+STATS_AREA_HEIGHT+1, mask);
        break;
    case AURA_HORN:
        screenShowChar(1, STATS_AREA_X + STATS_AREA_WIDTH/2, STATS_AREA_Y+STATS_AREA_HEIGHT+1);
        break;
    case AURA_JINX:
        screenShowChar('J', STATS_AREA_X + STATS_AREA_WIDTH/2, STATS_AREA_Y+STATS_AREA_HEIGHT+1);
        break;
    case AURA_NEGATE:
        screenShowChar('N', STATS_AREA_X + STATS_AREA_WIDTH/2, STATS_AREA_Y+STATS_AREA_HEIGHT+1);
        break;
    case AURA_PROTECTION:
        screenShowChar('P', STATS_AREA_X + STATS_AREA_WIDTH/2, STATS_AREA_Y+STATS_AREA_HEIGHT+1);
        break;
    case AURA_QUICKNESS:
        screenShowChar('Q', STATS_AREA_X + STATS_AREA_WIDTH/2, STATS_AREA_Y+STATS_AREA_HEIGHT+1);
        break;
    }
}

void statsHighlightCharacter(int player) {
    ASSERT(player < c->saveGame->members, "player number out of range: %d", player);
    screenInvertRect(STATS_AREA_X * CHAR_WIDTH, (STATS_AREA_Y + player) * CHAR_HEIGHT, 
                     STATS_AREA_WIDTH * CHAR_WIDTH, CHAR_HEIGHT);
}

void statsAreaClear() {
    int i;

    for (i = 0; i < STATS_AREA_WIDTH; i++)
        screenTextAt(STATS_AREA_X + i, 0, "%c", 13);

    screenEraseTextArea(STATS_AREA_X, STATS_AREA_Y, STATS_AREA_WIDTH, STATS_AREA_HEIGHT);
}

/**
 * Sets the title of the stats area.
 */
void statsAreaSetTitle(const char *title) {
    int titleStart;

    titleStart = (STATS_AREA_WIDTH / 2) - ((strlen(title) + 2) / 2);
    screenTextAt(STATS_AREA_X + titleStart, 0, "%c%s%c", 16, title, 17);
}

/**
 * The basic party view.
 */
void statsShowPartyView(int member) {
    int i;
    char *format = "%d%c%-9.8s%3d%c";

    ASSERT(c->saveGame->members <= 8, "party members out of range: %d", c->saveGame->members);
    ASSERT(member <= 8, "party member out of range: %d", member);

    if (member == -1) {
        for (i = 0; i < c->saveGame->members; i++)
            screenTextAt(STATS_AREA_X, STATS_AREA_Y+i, format, i+1, (i==c->location->activePlayer) ? CHARSET_BULLET : '-', c->saveGame->players[i].name, c->saveGame->players[i].hp, c->saveGame->players[i].status);
    }
    else screenTextAt(STATS_AREA_X, STATS_AREA_Y+member, format, member+1, (member==c->location->activePlayer) ? CHARSET_BULLET : '-', c->saveGame->players[member].name, c->saveGame->players[member].hp, c->saveGame->players[member].status);
}

/**
 * The individual character view.
 */
void statsShowCharDetails(int charNo) {
    const char *classString;
    int classStart;

    ASSERT(charNo < 8, "character number out of range: %d", charNo);

    statsAreaSetTitle(c->saveGame->players[charNo].name);
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+0, "%c             %c", c->saveGame->players[charNo].sex, c->saveGame->players[charNo].status);
    classString = getClassName(c->saveGame->players[charNo].klass);
    classStart = (STATS_AREA_WIDTH / 2) - (strlen(classString) / 2);
    screenTextAt(STATS_AREA_X + classStart, STATS_AREA_Y, "%s", classString);
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+2, " MP:%02d  LV:%d", c->saveGame->players[charNo].mp, playerGetRealLevel(&c->saveGame->players[charNo]));
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+3, "STR:%02d  HP:%04d", c->saveGame->players[charNo].str, c->saveGame->players[charNo].hp);
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+4, "DEX:%02d  HM:%04d", c->saveGame->players[charNo].dex, c->saveGame->players[charNo].hpMax);
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+5, "INT:%02d  EX:%04d", c->saveGame->players[charNo].intel, c->saveGame->players[charNo].xp);
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+6, "W:%s", weaponGetName(c->saveGame->players[charNo].weapon));
    screenTextAt(STATS_AREA_X, STATS_AREA_Y+7, "A:%s", armorGetName(c->saveGame->players[charNo].armor));
}

/**
 * Weapons in inventory.
 */
void statsShowWeapons() {
    int w, line, col;
    extern int numWeapons;

    statsAreaSetTitle("Weapons");

    line = STATS_AREA_Y;
    col = 0;
    screenTextAt(STATS_AREA_X, line++, "A-%s", weaponGetName(WEAP_HANDS));
    for (w = WEAP_HANDS + 1; w < numWeapons; w++) {
        int n = c->saveGame->weapons[w];
        if (n >= 100)
            n = 99;
        if (n >= 1) {
            char *format = (n >= 10) ? "%c%d-%s" : "%c-%d-%s";

            screenTextAt(STATS_AREA_X + col, line++, format, w - WEAP_HANDS + 'A', n, weaponGetAbbrev((WeaponType) w));
            if (line >= (STATS_AREA_Y+STATS_AREA_HEIGHT)) {
                line = STATS_AREA_Y;
                col += 8;
            }
        }
    }
}

/**
 * Armor in inventory.
 */
void statsShowArmor() {
    int a, line;

    statsAreaSetTitle("Armour");

    line = STATS_AREA_Y;
    screenTextAt(STATS_AREA_X, line++, "A  -No Armour");
    for (a = ARMR_NONE + 1; a < ARMR_MAX; a++) {
        if (c->saveGame->armor[a] > 0) {
            char *format = (c->saveGame->armor[a] >= 10) ? "%c%d-%s" : "%c-%d-%s";

            screenTextAt(STATS_AREA_X, line++, format, a - ARMR_NONE + 'A', c->saveGame->armor[a], armorGetName((ArmorType) a));
        }
    }
}

/**
 * Equipment: touches, gems, keys, and sextants.
 */
void statsShowEquipment() {
    int line;

    statsAreaSetTitle("Equipment");

    line = STATS_AREA_Y;
    screenTextAt(STATS_AREA_X, line++, "%2d Torches", c->saveGame->torches);
    screenTextAt(STATS_AREA_X, line++, "%2d Gems", c->saveGame->gems);
    screenTextAt(STATS_AREA_X, line++, "%2d Keys", c->saveGame->keys);
    if (c->saveGame->sextants > 0)
        screenTextAt(STATS_AREA_X, line++, "%2d Sextants", c->saveGame->sextants);
}

/**
 * Items: runes, stones, and other miscellaneous quest items.
 */
void statsShowItems() {
    int i, j;
    int line;
    char buffer[17];

    statsAreaSetTitle("Items");

    line = STATS_AREA_Y;
    if (c->saveGame->stones != 0) {
        j = 0;
        for (i = 0; i < 8; i++) {
            if (c->saveGame->stones & (1 << i))
                buffer[j++] = getStoneName((Virtue) i)[0];
        }
        buffer[j] = '\0';
        screenTextAt(STATS_AREA_X, line++, "Stones:%s", buffer);
    }
    if (c->saveGame->runes != 0) {
        j = 0;
        for (i = 0; i < 8; i++) {
            if (c->saveGame->runes & (1 << i))
                buffer[j++] = getVirtueName((Virtue) i)[0];
        }
        buffer[j] = '\0';
        screenTextAt(STATS_AREA_X, line++, "Runes:%s", buffer);
    }
    if (c->saveGame->items & (ITEM_CANDLE | ITEM_BOOK | ITEM_BELL)) {
        buffer[0] = '\0';
        if (c->saveGame->items & ITEM_BELL) {
            strcat(buffer, getItemName(ITEM_BELL));
            strcat(buffer, " ");
        }
        if (c->saveGame->items & ITEM_BOOK) {
            strcat(buffer, getItemName(ITEM_BOOK));
            strcat(buffer, " ");
        }
        if (c->saveGame->items & ITEM_CANDLE) {
            strcat(buffer, getItemName(ITEM_CANDLE));
            buffer[15] = '\0';
        }
        screenTextAt(STATS_AREA_X, line++, "%s", buffer);
    }
    if (c->saveGame->items & (ITEM_KEY_C | ITEM_KEY_L | ITEM_KEY_T)) {
        j = 0;
        if (c->saveGame->items & ITEM_KEY_T)
            buffer[j++] = getItemName(ITEM_KEY_T)[0];
        if (c->saveGame->items & ITEM_KEY_L)
            buffer[j++] = getItemName(ITEM_KEY_L)[0];
        if (c->saveGame->items & ITEM_KEY_C)
            buffer[j++] = getItemName(ITEM_KEY_C)[0];
        buffer[j] = '\0';
        screenTextAt(STATS_AREA_X, line++, "3 Part Key:%s", buffer);
    }
    if (c->saveGame->items & ITEM_HORN)
        screenTextAt(STATS_AREA_X, line++, "%s", getItemName(ITEM_HORN));
    if (c->saveGame->items & ITEM_WHEEL)
        screenTextAt(STATS_AREA_X, line++, "%s", getItemName(ITEM_WHEEL));
    if (c->saveGame->items & ITEM_SKULL)
        screenTextAt(STATS_AREA_X, line++, "%s", getItemName(ITEM_SKULL));
}

/**
 * Unmixed reagents in inventory.
 */
void statsShowReagents() {
    int r, line;    
    extern Menu spellMixMenu;

    statsAreaSetTitle("Reagents");    
    
    line = STATS_AREA_Y;
    for (r = REAG_ASH; r < REAG_MAX; r++) {
        int n = c->saveGame->reagents[r];
        if (n >= 100)
            n = 99;

        /* show the quantity for the item */
        if (n > 0) {          
            screenTextAt(STATS_AREA_X, line, "%c-", r+'A');
            screenTextAt(STATS_AREA_X+13, line++, "%2d", n);
        }
    }

    menuShow(menuGetRoot(spellMixMenu));
}

/**
 * Mixed reagents in inventory.
 */
void statsShowMixtures() {
    int s, line, col;

    statsAreaSetTitle("Mixtures");

    line = STATS_AREA_Y;
    col = 0;
    for (s = 0; s < SPELL_MAX; s++) {
        int n = c->saveGame->mixtures[s];
        if (n >= 100)
            n = 99;
        if (n >= 1) {
            screenTextAt(STATS_AREA_X + col, line++, "%c-%02d", s + 'A', n);
            if (line >= (STATS_AREA_Y+STATS_AREA_HEIGHT)) {
                if (col >= 10)
                    break;
                line = STATS_AREA_Y;
                col += 5;
            }
        }
    }
}
