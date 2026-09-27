// Penumbra e o Castelo das Sombras - Enhanced, on the Supersonic Engine.

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/GameRuntime.hpp"
#include "core/LaunchOptions.hpp"
#include "core/Log.hpp"
#include "core/SupersonicApp.hpp"
#include "platform/ExecutablePath.hpp"
#include "renderer/VulkanContext.hpp"

#include "PenumbraLayer.hpp"

int main(int argc, char** argv) {
    // The game's own flags come out first: LaunchOptions refuses anything it
    // does not know, which is right for the engine.
    Penumbra::PenumbraLayer::Options layerOptions;
    std::vector<char*> engineArgs{argv[0]};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--start" && i + 1 < argc) {
            layerOptions.startScene = argv[++i];
            continue;
        }
        engineArgs.push_back(argv[i]);
    }

    Supersonic::LaunchOptions options;
    try {
        options = Supersonic::LaunchOptions::Parse(static_cast<int>(engineArgs.size()), engineArgs.data());
    } catch (const std::exception& e) {
        std::cerr << "[Penumbra] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    Supersonic::GameManifest manifest;
    manifest.isGame = true;
    manifest.title = "Penumbra";
    manifest.startupScene.clear();     // the layer builds the world
    manifest.width = 1366;
    manifest.height = 768;

    const std::filesystem::path userDir = Supersonic::UserDataDirectory(manifest.title);
    layerOptions.userDir = userDir.string();

    try {
        Supersonic::SupersonicApp app(options, &manifest);
        SUPERSONIC_LOG_INFO("Penumbra")
            << "Vulkan validation layers: "
            << (Supersonic::VulkanContext::ValidationLayersActive() ? "ACTIVE"
                                                                    : "NOT LOADED - nothing is checking this run")
            << std::endl;
        app.PushLayer(std::make_unique<Penumbra::PenumbraLayer>(layerOptions));
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "[Penumbra] fatal: " << e.what() << std::endl;
        Supersonic::Log::CloseFileSink();
        return EXIT_FAILURE;
    }

    const unsigned validationErrors = Supersonic::VulkanContext::ValidationErrorCount();
    Supersonic::Log::CloseFileSink();
    if (validationErrors > 0) {
        std::cerr << "[Penumbra] " << validationErrors << " Vulkan validation error(s); failing the run." << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
