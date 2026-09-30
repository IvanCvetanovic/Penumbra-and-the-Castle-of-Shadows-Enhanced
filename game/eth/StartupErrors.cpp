#include "eth/StartupErrors.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Penumbra::Eth {

namespace {

// Portuguese letters as UTF-8 bytes: the sources are ASCII, and a \u escape in a
// narrow literal would come out in MSVC's execution code page (cp1252), not
// UTF-8. Each is a literal of its own, so a following letter that happens to be
// a hex digit ("gr\xC3\xA1" "ficos") can never join the escape.
#define PT_A_ACUTE "\xC3\xA1"
#define PT_A_TILDE "\xC3\xA3"
#define PT_C_CEDIL "\xC3\xA7"
#define PT_I_ACUTE "\xC3\xAD"
#define PT_O_TILDE "\xC3\xB5"
#define PT_U_ACUTE "\xC3\xBA"

constexpr const char* kIssuesUrl = "github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/issues";

struct Texts {
    const char* english;
    const char* portuguese;
};

Texts TextsFor(StartupProblem problem) {
    switch (problem) {
    case StartupProblem::GameFilesMissing:
        return {"Penumbra could not find its game files.\n\n"
                "Please extract (unzip) the whole folder first, then run Penumbra.exe from there: "
                "right-click the zip file, choose \"Extract All\", open the folder it makes, then the "
                "Penumbra folder, and double-click Penumbra.exe.",
                "O Penumbra n" PT_A_TILDE "o encontrou os arquivos do jogo.\n\n"
                "Primeiro extraia (descompacte) a pasta inteira e depois abra o Penumbra.exe de dentro dela: "
                "clique com o bot" PT_A_TILDE "o direito no arquivo zip, escolha \"Extrair Tudo\", abra a pasta "
                "criada, depois a pasta Penumbra, e clique duas vezes em Penumbra.exe."};
    case StartupProblem::FolderNameUnusable:
        return {"Penumbra cannot run from this folder: its name, or the name of a folder above it, has "
                "letters this PC's language settings cannot read.\n\n"
                "Please move the Penumbra folder to a simpler place, for example C:\\Games\\Penumbra, "
                "and run Penumbra.exe from there.",
                "O Penumbra n" PT_A_TILDE "o pode ser aberto desta pasta: o nome dela, ou de uma pasta acima "
                "dela, tem letras que as configura" PT_C_CEDIL PT_O_TILDE "es de idioma deste PC n" PT_A_TILDE
                "o conseguem ler.\n\n"
                "Mova a pasta Penumbra para um lugar mais simples, por exemplo C:\\Jogos\\Penumbra, "
                "e abra o Penumbra.exe de l" PT_A_ACUTE "."};
    case StartupProblem::NoGraphics:
        return {"Penumbra could not start its graphics.\n\n"
                "The game needs a graphics card with Vulkan 1.2 - most PCs from about the last eight years "
                "have one. Please update the graphics driver (through Windows Update, or from the NVIDIA, "
                "AMD or Intel website) and try again. A very old PC may not be able to run the game.",
                "O Penumbra n" PT_A_TILDE "o conseguiu iniciar os gr" PT_A_ACUTE "ficos.\n\n"
                "O jogo precisa de uma placa de v" PT_I_ACUTE "deo com Vulkan 1.2 - a maioria dos PCs dos "
                PT_U_ACUTE "ltimos oito anos tem uma. Atualize o driver de v" PT_I_ACUTE "deo (pelo Windows "
                "Update ou pelo site da NVIDIA, AMD ou Intel) e tente de novo. Um PC muito antigo pode "
                "n" PT_A_TILDE "o conseguir rodar o jogo."};
    case StartupProblem::StoppedByError:
        break;
    }
    return {"Penumbra stopped because of an unexpected error. Sorry!\n\n"
            "If it happens again, please report it at ",
            "O Penumbra parou por causa de um erro inesperado. Desculpe!\n\n"
            "Se acontecer de novo, por favor avise em "};
}

#undef PT_A_ACUTE
#undef PT_A_TILDE
#undef PT_C_CEDIL
#undef PT_I_ACUTE
#undef PT_O_TILDE
#undef PT_U_ACUTE

} // namespace

bool ShouldShowStartupDialog(bool headless, ErrorStream stderrStream) {
    // Only a run with no stderr at all: one started from Explorer or a
    // shortcut, where the player could never read the line. A console, a pipe
    // or a file means a shell or a program started it and reads it there.
    return !headless && stderrStream == ErrorStream::Nowhere;
}

std::string StartupMessage(StartupProblem problem, bool portugueseFirst, const std::string& detail,
                           const std::string& logPath) {
    const Texts texts = TextsFor(problem);
    std::string english = texts.english;
    std::string portuguese = texts.portuguese;
    if (problem == StartupProblem::StoppedByError) {
        english += std::string(kIssuesUrl) + ".";
        portuguese += std::string(kIssuesUrl) + ".";
    }
    std::string text = portugueseFirst ? portuguese : english;
    text += "\n\n----------------------------------------\n\n";
    text += portugueseFirst ? english : portuguese;
    if (!detail.empty()) text += "\n\nDetails: " + detail;
    if (!logPath.empty()) text += std::string(detail.empty() ? "\n\n" : "\n") + "Log: " + logPath;
    return text;
}

#ifdef _WIN32

ErrorStream CurrentErrorStream() {
    const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return ErrorStream::Nowhere;
    switch (GetFileType(handle)) {
    case FILE_TYPE_PIPE: return ErrorStream::Pipe;
    case FILE_TYPE_DISK: return ErrorStream::File;
    case FILE_TYPE_CHAR: return ErrorStream::Console;
    default: return ErrorStream::Nowhere;
    }
}

bool VulkanLoaderAvailable() {
    // The same search the delay-load helper makes, and the module stays loaded
    // for it: the first Vulkan call then binds to this very DLL.
    return LoadLibraryExW(L"vulkan-1.dll", nullptr, 0) != nullptr;
}

void ShowStartupDialog(const std::string& utf8Text) {
    std::wstring wide;
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), nullptr, 0);
    if (length > 0) {
        wide.resize(static_cast<std::size_t>(length));
        MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), wide.data(), length);
    }
    std::wstring caption;
    for (const char* c = kStartupDialogTitle; *c != '\0'; ++c) caption += static_cast<wchar_t>(*c);
    MessageBoxW(nullptr, wide.c_str(), caption.c_str(), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

#else

// No dialog exists off Windows, so no stream there asks for one.
ErrorStream CurrentErrorStream() { return ErrorStream::Pipe; }

bool VulkanLoaderAvailable() { return true; }

void ShowStartupDialog([[maybe_unused]] const std::string& utf8Text) {}

#endif

} // namespace Penumbra::Eth
