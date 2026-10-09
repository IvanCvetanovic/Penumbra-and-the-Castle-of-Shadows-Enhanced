#include "eth/AudioPrefetch.hpp"

#include <filesystem>
#include <utility>

namespace Penumbra::Eth {

AudioPrefetch::AudioPrefetch(Decoder decoder) : m_decoder(std::move(decoder)) {}

AudioPrefetch::~AudioPrefetch() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_changed.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

// One spelling per file, so a path asked for as the scripts spell it finds the one the directory listing queued.
std::string AudioPrefetch::Key(const std::string& path) {
    return std::filesystem::path(path).lexically_normal().generic_string();
}

void AudioPrefetch::Request(const std::vector<std::string>& paths) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const std::string& path : paths) {
            const std::string key = Key(path);
            if (m_entries.count(key) != 0) continue;
            m_entries.emplace(key, Entry{});
            m_queue.push_back(key);
        }
        if (!m_worker.joinable() && !m_queue.empty()) m_worker = std::thread([this] { Work(); });
    }
    m_changed.notify_all();
}

void AudioPrefetch::Work() {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        m_changed.wait(lock, [this] { return m_stop || !m_queue.empty(); });
        if (m_stop) return;
        const std::string key = m_queue.front();
        m_queue.pop_front();
        const auto found = m_entries.find(key);
        // Taken off the queue by a Take that decodes it itself.
        if (found == m_entries.end() || found->second.state != State::Queued) continue;
        found->second.state = State::Decoding;

        // Decoding takes seconds; the game thread must be able to Take and Request meanwhile. The entry is a node of a std::map,
        // so a reference to it stays valid while others are inserted.
        Entry& entry = found->second;
        lock.unlock();
        Supersonic::AudioClip clip;
        std::string error;
        const bool decoded = m_decoder(key, clip, error);
        lock.lock();
        if (decoded) {
            entry.clip = std::move(clip);
            entry.state = State::Done;
        } else {
            entry.error = std::move(error);
            entry.state = State::Failed;
        }
        m_changed.notify_all();
    }
}

bool AudioPrefetch::Take(const std::string& path, Supersonic::AudioClip& out, std::string& error) {
    const std::string key = Key(path);
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto found = m_entries.find(key);
    if (found == m_entries.end()) return false;
    Entry& entry = found->second;
    if (entry.state == State::Queued) {
        // Not started: the caller is here now and decodes it itself, and the worker must not do it a second time.
        entry.state = State::Claimed;
        return false;
    }
    if (entry.state == State::Claimed) return false;
    // At most the remainder of the one file the worker is on.
    m_changed.wait(lock, [&entry] { return entry.state != State::Decoding; });
    const bool done = entry.state == State::Done;
    if (done) {
        out = std::move(entry.clip);
    } else {
        error = entry.error;
    }
    m_entries.erase(found);
    return done;
}

int AudioPrefetch::Queued() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& item : m_entries) count += item.second.state == State::Queued ? 1 : 0;
    return count;
}

int AudioPrefetch::Ready() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& item : m_entries) count += item.second.state == State::Done ? 1 : 0;
    return count;
}

} // namespace Penumbra::Eth
