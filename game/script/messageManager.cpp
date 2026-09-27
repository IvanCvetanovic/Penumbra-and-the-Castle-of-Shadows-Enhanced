// messageManager.as, ported line by line: the on-screen message list (help
// texts, warnings) and the floating damage numbers. The original is
// extracted/app/messageManager.as, part of Penumbra (Andre Santee, 2010), free
// software under the GNU Lesser General Public License, version 3 or (at your
// option) any later version.
//
// Message (messageManager.as:43-57) is a plain record with a defaulted
// constructor in Script.hpp.

#include "script/Script.hpp"

namespace Penumbra::Script {

// messageManager.as:61. maxMessages = 10, maxDamageMessages = 15,
// messageTime = 4000 and damageIndex = 0 (:63-67) are the members' initializers
// in Script.hpp, so they already hold when the damage ring is sized here.
MessageManager::MessageManager()
{
    m_damage.resize(m_maxDamageMessages);                             // messageManager.as:66
}

// messageManager.as:70. A damage number: overwrites the next slot of the ring,
// at a world position above `origin`.
void MessageManager::addMessage(const int damageValue, const vector2& origin)
{
    if (m_damageIndex >= m_maxDamageMessages)
        m_damageIndex = 0;

    m_damage[m_damageIndex].time = GetTime();
    m_damage[m_damageIndex].message = ""+Str(damageValue);
    m_damage[m_damageIndex].alpha = 255;
    m_damage[m_damageIndex].origin = origin+vector2(-10,-32);
    m_damageIndex++;
}

// messageManager.as:82. The help sound plays whenever msg differs from the
// newest message shown, BEFORE the duplicate check: a message already listed
// returns without refreshing its time, and a full list drops it silently.
void MessageManager::addMessage(const string& msg)
{
    if (!SampleExists("soundfx/help.mp3"))
    {
        LoadSoundEffect("soundfx/help.mp3");
    }
    if (/*!IsSamplePlaying("soundfx/help.mp3")*/
        m_lastMessage != msg)
    {
        PlaySample("soundfx/help.mp3");
    }

    uint t;
    bool messageExists = false;
    for (t=0; t<m_maxMessages; t++)                                   // messageManager.as:96
    {
        const string key = getKey(t);
        std::shared_ptr<Message> msgHandle;
        if (m_dict.get(key, msgHandle))
        {
            if (msg == msgHandle->message)
            {
                messageExists = true;
                return;
            }
        }
    }

    // A handle-held object, as AngelScript's `Message newMsg; ... @newMsg`
    // (messageManager.as:110, :121): the dictionary keeps it alive.
    auto newMsg = std::make_shared<Message>();
    newMsg->time = GetTime();
    newMsg->message = msg;

    if (!messageExists)
    {
        for (t=0; t<m_maxMessages; t++)                               // messageManager.as:116
        {
            const string key = getKey(t);
            if (!m_dict.exists(key))
            {
                m_dict.set(key, newMsg);
                return;
            }
        }
    }
}

// messageManager.as:128. Messages are placed by SLOT index (an empty slot
// leaves a gap); the newest one is echoed at half alpha under player 0; damage
// numbers are drawn in world space for a third of messageTime.
void MessageManager::showMessages(const vector2& pos, const string& font, const float size,
                                  uint8 r, uint8 g, uint8 b, CameraManager& camera)
{
    processMessages();
    std::shared_ptr<Message> latest = nullptr;                        // messageManager.as:132
    const float smallerTextSize = size/2;
    for (uint t=0; t<m_maxMessages; t++)
    {
        const string key = getKey(t);
        std::shared_ptr<Message> msg;
        if (m_dict.get(key, msg))
        {
            if (latest == nullptr)
            {
                latest = msg;
                m_lastMessage = msg->message;
            }
            // <= : of equal times, the later slot wins (messageManager.as:145).
            if (latest->time <= msg->time)
            {
                m_lastMessage = msg->message;
                latest = msg;
            }
            const vector2 msgPos = pos+vector2(0,static_cast<float>(t)*size);
            shadowText(msgPos, msg->message, font, size, msg->alpha,r,g,b);
        }
    }
    if (latest != nullptr)                                            // messageManager.as:154
    {
        vector2 charPos = camera.getMainCharPos(0)-GetCameraPos();
        charPos.x -= 64;
        charPos.y += 32;
        shadowText(charPos, latest->message, font, smallerTextSize, static_cast<uint8>(latest->alpha/2),r,g,b);
    }
    else
    {
        // An empty list forgets the last message, so re-adding the same text
        // plays the help sound again (messageManager.as:89).
        m_lastMessage = "";
    }

    // A slot never written has time 0, so for the first messageTime/3 ms of
    // the app the 15 empty slots are drawn too, as empty text
    // (messageManager.as:167-174).
    const float damageTextSize = size/2;
    for (uint t=0; t<m_maxDamageMessages; t++)                        // messageManager.as:167
    {
        const uint elapsed = GetTime()-m_damage[t].time;
        if (elapsed < m_messageTime/3)
        {
            shadowText(m_damage[t].origin-GetCameraPos(), m_damage[t].message, font, damageTextSize, m_damage[t].alpha,r,g,b);
        }
    }
}

// messageManager.as:177. Fades each message over messageTime and deletes it
// then; fades each damage number over messageTime/3 while it rises 15 px/s.
void MessageManager::processMessages()
{
    for (uint t=0; t<m_maxMessages; t++)
    {
        const string key = getKey(t);
        std::shared_ptr<Message> msg;
        if (m_dict.get(key, msg))
        {
            const uint elapsed = (GetTime()-msg->time);
            if (elapsed < m_messageTime)
            {
                // uint math, as the original; the value is in [1,255].
                msg->alpha = static_cast<uint8>(255-((elapsed*255)/m_messageTime));   // messageManager.as:188
            }
            else
            {
                m_dict.deleteKey(key);                                // messageManager.as:192
            }
        }
    }

    for (uint t=0; t<m_maxDamageMessages; t++)                        // messageManager.as:197
    {
        const uint elapsed = GetTime()-m_damage[t].time;
        if (elapsed < m_messageTime/3)
        {
            m_damage[t].alpha = static_cast<uint8>(255-((elapsed*255)/(m_messageTime/3)));   // messageManager.as:202
            m_damage[t].origin.y -= UnitsPerSecond(15);
        }
    }
}

// messageManager.as:208. The dictionary keys "k0".."k9".
string MessageManager::getKey(const uint n)
{
    return ("k" + Str(n));
}

} // namespace Penumbra::Script
