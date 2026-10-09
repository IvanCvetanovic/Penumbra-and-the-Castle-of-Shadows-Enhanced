#pragma once

// SOUNDS DECODED IN THE BACKGROUND, so the thread that reads the touch screen is not away for seconds.
//
// The engine decodes a whole file to PCM on first use, and the game thread is the one that asks, inside the tick of a scene load.
// A level asks for about four minutes of MP3 and forty seconds of Ogg at once (setupScene's LoadMusic and LoadSoundEffect), and
// on a phone the game thread is also the only reader of the input queue: for the length of that decode no touch is read and the
// picture does not move - "my touchscreen completely stops working", once on each cold start, at the first New Game.
//
// This is a worker that decodes a list of files while the menu is up, in order, and hands each clip over when the game thread
// asks for it. It never makes anything slower than decoding it inline: a file the worker has not started is taken off its queue
// and decoded by the caller as before; one it is decoding is waited for (at most that one file's remainder); one it has finished
// is moved out at once. The decoder is a parameter (Eth::LoadSound in the game) so the worker's behaviour is tested without
// files, and so it stays out of Windows, whose MP3 decoder (Media Foundation) wants a thread set up for it.

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/AudioClip.hpp"

namespace Penumbra::Eth {

class AudioPrefetch {
public:
    // Fills `out` from the file at `path`; false with `error` set when it will not decode. Called on the worker's thread, so it
    // must not touch anything the game thread does.
    using Decoder = std::function<bool(const std::string& path, Supersonic::AudioClip& out, std::string& error)>;

    explicit AudioPrefetch(Decoder decoder);
    // Stops after the file being decoded (a decode cannot be interrupted) and joins.
    ~AudioPrefetch();
    AudioPrefetch(const AudioPrefetch&) = delete;
    AudioPrefetch& operator=(const AudioPrefetch&) = delete;

    // Queues `paths`, in the order given, behind what is queued. A path already requested and not yet taken is ignored. Starts the
    // worker on the first call. Any thread.
    void Request(const std::vector<std::string>& paths);

    // The decoded clip of `path`, moved into `out`: true when the worker has it, or is decoding it now (this waits for that one
    // file). False - and the caller decodes it itself - when `path` was never requested, when the worker had not started on it
    // (it is taken off the queue, so the worker will not decode it too), or when its decode failed (`error` says why; it is
    // forgotten, so a second ask is the caller's own try). Any thread.
    bool Take(const std::string& path, Supersonic::AudioClip& out, std::string& error);

    // For a test and for a log line: files queued and not started, and files decoded and not yet taken.
    int Queued() const;
    int Ready() const;

private:
    enum class State { Queued, Decoding, Done, Failed, Claimed };
    struct Entry {
        State state = State::Queued;
        Supersonic::AudioClip clip;
        std::string error;
    };

    static std::string Key(const std::string& path);
    void Work();

    Decoder m_decoder;
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::map<std::string, Entry> m_entries;
    std::deque<std::string> m_queue;   // keys, in order
    std::thread m_worker;
    bool m_stop = false;
};

} // namespace Penumbra::Eth
