// scores.as, ported (extracted/app/scores.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The five best campaign times, in milliseconds, in hs.enml as hs0..hs4
// (ascending: hs0 is the best). GetAbsolutePath("hs.enml") names the original's
// file; the Eth layer reads the user directory's copy when there is one and
// redirects the write there, so extracted/app is never written.
//
// The typed ENML getters leave `score` unwritten when a key is missing
// (E:enml.h:663-712), so a missing hsN repeated the previous one in the first
// port of this file (AngelScript 2.20 copied an &out temporary back instead,
// which is 0). E36 starts every key from the shipped file's 59:59 instead, so
// a list with a key missing, or no list at all, reads as the file did when
// the game was installed.
//
// ENHANCEMENT E36 (not in the original): two difficulties keep a list each.
// Normal's is the original's entity "hs" (a file written before E36 is read
// as Normal's list, untouched); Hard's is a second entity in the same file,
// "hsHard", with the same keys. The functions without a difficulty are the
// original's, for Normal; getGetBestTime() is the better of the two.

#include "script/Script.hpp"

namespace Penumbra::Script {

namespace {

// E36: the entity of hs.enml that holds a difficulty's five times.
const char* recordEntity(const uint difficulty)                // E36
{
    return difficulty == DIFFICULTY_HARD ? "hsHard" : "hs";
}

// E36: one key of a list. It starts at the default, because the typed getter
// leaves it alone when the key (or the whole entity, Hard's in a file written
// before E36) is missing: 0 would unlock every locked arena, and the previous
// key's time would be somebody else's record.
uint readRecord(const enmlFile& file, const char* entity, const uint t)   // E36
{
    uint score = DEFAULT_RECORD_TIME;
    file.getUint(entity, "hs" + Str(t), score);
    return score;
}

array<uint> readRecordList(const enmlFile& file, const char* entity)      // E36
{
    array<uint> list(MAX_SCORES);
    for (uint t=0; t<MAX_SCORES; t++)
        list[t] = readRecord(file, entity, t);
    return list;
}

// E36: a list as the file keeps it, hs0..hs4.
void putRecordList(enmlFile& file, const uint difficulty, const array<uint>& list)   // E36
{
    enmlEntity entity;
    for (uint t=0; t<MAX_SCORES; t++)
        entity.add("hs" + Str(t), ""+Str(list[t]));
    file.addEntity(recordEntity(difficulty), entity);
}

} // namespace

// E36: "Normal" / "Dif\xEDcil", the words every screen names a difficulty by
// (the headings of the best times, the heading of the end screen's list, the
// New Game panel's line). One spelling, so that one translation serves all.
string difficultyName(const uint difficulty)
{
    return difficulty == DIFFICULTY_HARD ? string("Dif\xED" "cil") : string("Normal");
}

// scores.as:45. E36: `difficulty` picks the list the time goes into. Both lists
// are read before the file is cleared and both written back, so a Normal time
// never wipes Hard's list nor the reverse.
bool addNewRecordTime(const uint elapsed, const uint difficulty)
{
    bool r = false;
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    array<uint> normal = readRecordList(highScores, recordEntity(DIFFICULTY_NORMAL));   // E36
    array<uint> hard = readRecordList(highScores, recordEntity(DIFFICULTY_HARD));       // E36
    array<uint>& scores = difficulty == DIFFICULTY_HARD ? hard : normal;                // E36

    array<uint> list(MAX_SCORES+1);

    for (uint t=0; t<MAX_SCORES; t++)
    {
        list[t] = scores[t];
    }

    list[MAX_SCORES] = elapsed;

    // scores.as:63-77: sorts ascending. The k loop does nothing but repeat the
    // test: after one swap list[i] >= list[j], so it never swaps back.
    for (uint i=0; i<=MAX_SCORES; i++)
    {
        for (uint j=0; j<=MAX_SCORES; j++)
        {
            for (uint k=0; k<=MAX_SCORES; k++)
            {
                if (list[i] < list[j])
                {
                    const uint temp = list[i];
                    list[i] = list[j];
                    list[j] = temp;
                }
            }
        }
    }

    highScores.clear();                                         // scores.as:79
    for (uint t=0; t<MAX_SCORES; t++)
    {
        if (list[t] == elapsed)
            r = true;
        scores[t] = list[t];                                    // E36: the chosen list now holds the new top five
    }
    putRecordList(highScores, DIFFICULTY_NORMAL, normal);       // E36
    putRecordList(highScores, DIFFICULTY_HARD, hard);           // E36
    highScores.writeToFile(GetAbsolutePath("hs.enml"));
    return r;
}

// scores.as:45, the original's function: a time of Normal's.
bool addNewRecordTime(const uint elapsed)
{
    return addNewRecordTime(elapsed, DIFFICULTY_NORMAL);        // E36
}

// scores.as:92. E36: one difficulty's list.
string getRecordTimeList(const uint difficulty)
{
    string r;
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    const array<uint> list = readRecordList(highScores, recordEntity(difficulty));   // E36

    for (uint t=0; t<MAX_SCORES; t++)
    {
        r += (Str(static_cast<uint>(t+1)) + "    " + getTimeString(list[t]) + "\n");
    }
    return r;
}

// scores.as:92, the original's function: Normal's list.
string getRecordTimeList()
{
    return getRecordTimeList(DIFFICULTY_NORMAL);                // E36
}

// scores.as:110. E36: one difficulty's best time. The original answered 0 when
// hs.enml was missing, which unlocked the locked arenas (an arena stays locked
// while its `score` <= this, menu.as:320, :366); the shipped file is always
// found in the original's folder, and the default is now 59:59 like its keys.
uint getGetBestTime(const uint difficulty)
{
    string r;   // scores.as:112: never used
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    return readRecord(highScores, recordEntity(difficulty), 0);   // E36: hs0
}

// scores.as:110, the original's function. E36: the better of the two lists'
// best times, so an arena's lock opens for a fast enough finish in either
// difficulty; with both lists at their defaults, as shipped, it is 59:59 and
// every locked arena stays locked.
uint getGetBestTime()
{
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    const uint normal = readRecord(highScores, recordEntity(DIFFICULTY_NORMAL), 0);   // E36
    const uint hard = readRecord(highScores, recordEntity(DIFFICULTY_HARD), 0);       // E36
    return hard < normal ? hard : normal;
}

} // namespace Penumbra::Script
