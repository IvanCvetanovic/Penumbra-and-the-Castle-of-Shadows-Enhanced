#pragma once

#include <string>

#include <entt/entt.hpp>

#include "core/EngineLayer.hpp"

namespace Penumbra {

// Penumbra as the engine plays it: one Ethanon frame (eth/Machine.hpp) per
// 60 Hz tick, and the snapshot that frame leaves drawn every frame.
class PenumbraLayer final : public Supersonic::EngineLayer {
public:
    static constexpr float kTick = 1.0f / 60.0f;

    struct Options {
        std::string userDir;        // where saves go; empty = none
        std::string startScene;     // "" = the menu, as the original boots
    };

    explicit PenumbraLayer(Options options);
    ~PenumbraLayer() override;

    const char* Name() const override { return "Penumbra"; }
    void OnAttach(entt::registry& registry) override;
    void OnDetach(entt::registry& registry) override;
    void OnFixedUpdate(entt::registry& registry, float fixedDelta) override;
    void OnUpdate(entt::registry& registry, float deltaTime) override;

private:
    Options m_options;
};

} // namespace Penumbra
