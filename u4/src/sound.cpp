/*
 * $Id$
 */

#include "vc6.h" // Fixes things if you're using VC6, does nothing if otherwise

#include <SDL.h>
#include <SDL_mixer.h>
#include <libxml/xmlmemory.h>

#include "sound.h"

#include "debug.h"
#include "error.h"
#include "settings.h"
#include "u4file.h"
#include "xml.h"

char *soundFilenames[SOUND_MAX];
Mix_Chunk *soundChunk[SOUND_MAX];

int soundInit() {
    Sound soundTrack;
    xmlDocPtr doc;
    xmlNodePtr root, node;

    /*
     * load sound track filenames from xml config file
     */

    doc = xmlParse("sound.xml");
    root = xmlDocGetRootElement(doc);
    if (xmlStrcmp(root->name, (const xmlChar *) "sound") != 0)
        errorFatal("malformed sound.xml");

    soundTrack = (Sound)0;
    for (node = root->xmlChildrenNode; node; node = node->next) {
        if (soundTrack >= SOUND_MAX)
            break;

        if (xmlNodeIsText(node) ||
            xmlStrcmp(node->name, (const xmlChar *) "track") != 0)
            continue;

        soundFilenames[soundTrack] = (char *)xmlGetProp(node, (const xmlChar *) "file");
        soundTrack = (Sound)(soundTrack + 1);
    }
    xmlFreeDoc(doc);

    return 1;
}

void soundDelete() {
}

void soundPlay(Sound sound) {

    ASSERT(sound < SOUND_MAX, "Attempted to play an invalid sound in soundPlay()");
    
    if (!settings.soundVol)
        return;

    if (soundChunk[sound] == NULL) {
        string pathname(u4find_sound(soundFilenames[sound]));
        if (!pathname.empty()) {
            soundChunk[sound] = Mix_LoadWAV(pathname.c_str());
            if (!soundChunk[sound]) {
                errorWarning("unable to load sound effect file %s: %s", soundFilenames[sound], Mix_GetError());
                return;
            }
        }
    }

    /**
     * Use Channel 1 for sound effects
     */ 
    if (!Mix_Playing(1)) {
        if (Mix_PlayChannel(1, soundChunk[sound], 0) == -1)
            errorWarning("error playing sound: %s", Mix_GetError());
    }
}
