// scores.as, ported (extracted/app/scores.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The five best campaign times, in milliseconds, in hs.enml as hs0..hs4
// (ascending: hs0 is the best). GetAbsolutePath("hs.enml") names the original's
// file; the Eth layer reads the user directory's copy when there is one and
// redirects the write there, so extracted/app is never written.
//
// The typed ENML getters leave `score` unwritten when a key is missing
// (E:enml.h:663-712), so a missing hsN repeats the previous one - here because
// `score` is passed by reference. AngelScript 2.20 copied an &out temporary
// back instead; the shipped hs.enml has every key, so the two never differ.

#include "script/Script.hpp"

namespace Penumbra::Script {

// scores.as:45
bool addNewRecordTime(const uint elapsed)
{
    bool r = false;
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    uint score = 0;
    array<uint> list(MAX_SCORES+1);

    for (uint t=0; t<MAX_SCORES; t++)
    {
        highScores.getUint("hs", "hs" + Str(t), score);
        list[t] = score;
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
    enmlEntity entity;
    for (uint t=0; t<MAX_SCORES; t++)
    {
        if (list[t] == elapsed)
            r = true;
        entity.add("hs" + Str(t), ""+Str(list[t]));
    }
    highScores.addEntity("hs", entity);
    highScores.writeToFile(GetAbsolutePath("hs.enml"));
    return r;
}

// scores.as:92
string getRecordTimeList()
{
    string r;
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    uint score = 0;

    for (uint t=0; t<MAX_SCORES; t++)
    {
        highScores.getUint("hs", "hs" + Str(t), score);
        r += (Str(static_cast<uint>(t+1)) + "    " + getTimeString(score) + "\n");
    }
    return r;
}

// scores.as:110. 0 when hs.enml is missing, which unlocks the locked arenas:
// an arena stays locked while its `score` <= this (menu.as:320, :366).
uint getGetBestTime()
{
    string r;   // scores.as:112: never used
    enmlFile highScores;
    const string str = GetStringFromFile(GetAbsolutePath("hs.enml"));
    highScores.parseString(str);

    uint score = 0;
    highScores.getUint("hs", "hs0", score);
    return score;
}

} // namespace Penumbra::Script
