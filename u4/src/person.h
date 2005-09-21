/*
 * $Id$
 */

#ifndef PERSON_H
#define PERSON_H

#include <list>
#include <string>

#include "creature.h"
#include "types.h"

using std::list;
using std::string;

class Conversation;
class Dialogue;

typedef enum {
   NPC_EMPTY,
   NPC_TALKER,
   NPC_TALKER_BEGGAR,
   NPC_TALKER_GUARD,
   NPC_TALKER_COMPANION,
   NPC_VENDOR_WEAPONS,
   NPC_VENDOR_ARMOR,
   NPC_VENDOR_FOOD,
   NPC_VENDOR_TAVERN,
   NPC_VENDOR_REAGENTS,
   NPC_VENDOR_HEALER,
   NPC_VENDOR_INN,
   NPC_VENDOR_GUILD,
   NPC_VENDOR_STABLE,
   NPC_LORD_BRITISH,
   NPC_HAWKWIND,
   NPC_MAX
} PersonNpcType;

class Person : public Creature {
public:
    Person(MapTile tile = 0) : Creature(tile) {
        setType(Object::PERSON);
    }

    bool canConverse() const;
    bool isVendor() const;

    list<string> getConversationText(Conversation *cnv, const char *inquiry);
    string getPrompt(Conversation *cnv);
    const char *getChoices(Conversation *cnv);

    string emptyGetIntro(Conversation *cnv);
    string talkerGetIntro(Conversation *cnv);
    string talkerGetResponse(Conversation *cnv, const char *inquiry);
    string talkerGetQuestionResponse(Conversation *cnv, const char *inquiry);
    string talkerGetPrompt(Conversation *cnv);
    string beggarGetQuantityResponse(Conversation *cnv, const char *response);
    string lordBritishGetIntro(Conversation *cnv);
    string lordBritishGetResponse(Conversation *cnv, const char *inquiry);
    string lordBritishGetQuestionResponse(Conversation *cnv, const char *answer);
    string lordBritishGetPrompt(const Conversation *cnv);
    string lordBritishGetHelp(const Conversation *cnv);
    string hawkwindGetIntro(Conversation *cnv);
    string hawkwindGetResponse(Conversation *cnv, const char *inquiry);
    string hawkwindGetPrompt(const Conversation *cnv);
    void getQuestion(Conversation *cnv, string *question);

public:
    string name;
    Dialogue* dialogue;
    MapCoords start;    
    PersonNpcType npcType;
};

bool isPerson(Object *punknown);

list<string> replySplit(const string &text);
int personInit(void);
int linecount(const string &s, int columnmax);

#endif
