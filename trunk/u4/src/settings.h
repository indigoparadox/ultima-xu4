/*
 * $Id$
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <map>
#include <string>
#include <vector>
#include "observable.h"
#include "types.h"

using std::string;

#define MIN_SHAKE_INTERVAL              50

#define MAX_BATTLE_SPEED                10
#define MAX_KEY_DELAY                   1000
#define MAX_KEY_INTERVAL                100
#define MAX_CYCLES_PER_SECOND           20
#define MAX_SPELL_EFFECT_SPEED          10
#define MAX_CAMP_TIME                   10
#define MAX_INN_TIME                    10
#define MAX_SHRINE_TIME                 20
#define MAX_SHAKE_INTERVAL              200

#define DEFAULT_SCALE                   2
#define DEFAULT_FULLSCREEN              0
#define DEFAULT_FILTER                  "Scale2x"
#define DEFAULT_VIDEO_TYPE              "VGA"
#define DEFAULT_GEM_LAYOUT              "Standard"
#define DEFAULT_LINEOFSIGHT             "DOS"
#define DEFAULT_SCREEN_SHAKES           1
#define DEFAULT_GAMMA                   100
#define DEFAULT_MUSIC_VOLUME            1
#define DEFAULT_SOUND_VOLUME            1
#define DEFAULT_VOLUME_FADES            1
#define DEFAULT_SHORTCUT_COMMANDS       0
#define DEFAULT_KEY_DELAY               500
#define DEFAULT_KEY_INTERVAL            30
#define DEFAULT_FILTER_MOVE_MESSAGES    0
#define DEFAULT_BATTLE_SPEED            5
#define DEFAULT_ENHANCEMENTS            1
#define DEFAULT_CYCLES_PER_SECOND       4
#define DEFAULT_DEBUG                   0
#define DEFAULT_VALIDATE_XML            1
#define DEFAULT_SPELL_EFFECT_SPEED      10
#define DEFAULT_CAMP_TIME               10
#define DEFAULT_INN_TIME                8
#define DEFAULT_SHRINE_TIME             16
#define DEFAULT_SHAKE_INTERVAL          100
#define DEFAULT_BATTLE_DIFFICULTY       "Normal"
#define DEFAULT_LOGGING                 ""

struct SettingsEnhancementOptions {
    bool activePlayer;
    bool u5spellMixing;
    bool u5shrines;
    bool u5combat;
    bool slimeDivides;
    bool gazerSpawnsInsects;
    bool c64chestTraps;    
    bool smartEnterKey;
    bool peerShowsObjects;
};

struct MouseOptions {
    bool enabled;
};

/**
 * SettingsData stores all the settings information.
 */
class SettingsData {
public:
    bool operator==(const SettingsData &) const;
    bool operator!=(const SettingsData &) const;

    int                 battleSpeed;
    bool                campingAlwaysCombat;
    int                 campTime;
    bool                debug;
    bool                enhancements;
    SettingsEnhancementOptions enhancementsOptions;    
    bool                filterMoveMessages;
    bool                fullscreen;
    int                 gameCyclesPerSecond;    
    bool                innAlwaysCombat;
    int                 innTime;
    int                 keydelay;
    int                 keyinterval;
    MouseOptions        mouseOptions;
    bool                musicVol;
    unsigned int        scale;
    bool                screenShakes;
    int                 gamma;
    int                 shakeInterval;
    bool                shortcutCommands;
    int                 shrineTime;
    bool                soundVol;
    int                 spellEffectSpeed;
    bool                validateXml;    
    bool                volumeFades;

    /**
     * Strings, classes, and other objects that cannot
     * be bitwise-compared must be placed here at the
     * end of the list so that our == and != operators
     * function correctly
     */ 
    long                end_of_bitwise_comparators;

    string              filter;
    string              gemLayout;
    string              lineOfSight;
    string              videoType;
    string              battleDiff;
    string              logging;
    string              game;
};

/**
 * The settings class is a singleton that holds all the settings
 * information.  It is dynamically initialized when first accessed.
 */ 
class Settings : public SettingsData, public Observable<Settings *> {
    typedef std::map<string, int, std::less<string> > SettingsMap;

public:
    /* Methods */
    static Settings &getInstance();
    void setData(const SettingsData &data);
    bool read();
    bool write();

    const string &getUserPath();
    const std::vector<string> &getBattleDiffs();

private:
    Settings();

    static Settings *instance;

    string userPath;
    string filename;
    std::vector<string> battleDiffs;
};

/* the global settings */
#define settings (Settings::getInstance())

#endif
