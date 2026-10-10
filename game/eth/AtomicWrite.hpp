#pragma once

// ENHANCEMENT E45 (not in the original, which opened its files truncated in place): a file the game saves is written whole
// or not at all.
//
// The bytes go to `<target>.tmp` and the temporary is renamed over `target`, so a write that is cut short (the process
// killed, no room left on the storage) leaves the old file as it was and a reader sees the whole of either file. Opened
// truncated in place, the same cut left a file that could not be read and had already replaced the last good one: a
// checkpoint that ended the run at the next death, a list of best times that read as never played.

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace Penumbra::Eth {

// False, with `why` set, when the temporary cannot be written or cannot be renamed over `target`; `target` is then
// untouched and no temporary is left behind.
inline bool WriteFileAtomic(const std::filesystem::path& target, const std::string& bytes, std::string& why) {
    std::filesystem::path partial = target;
    partial += ".tmp";
    std::error_code ec;
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) {
            why = "cannot write " + partial.string();
            return false;
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(partial, ec);
            why = "writing " + partial.string() + " failed";
            return false;
        }
    }
    std::filesystem::rename(partial, target, ec);
    if (ec) {
        const std::string message = ec.message();
        std::filesystem::remove(partial, ec);
        why = "cannot replace " + target.string() + ": " + message;
        return false;
    }
    return true;
}

}   // namespace Penumbra::Eth
