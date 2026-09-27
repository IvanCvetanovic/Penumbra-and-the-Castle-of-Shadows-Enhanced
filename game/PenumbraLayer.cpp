#include "PenumbraLayer.hpp"

#include <utility>

#include "core/SimulationClock.hpp"

namespace Penumbra {

PenumbraLayer::PenumbraLayer(Options options) : m_options(std::move(options)) {}

PenumbraLayer::~PenumbraLayer() = default;

void PenumbraLayer::OnAttach(entt::registry& registry) {
    // The original's frame ran once per 60 Hz vsync; its scripts count on it.
    if (auto* clock = registry.ctx().find<Supersonic::SimulationClock>()) clock->fixedDelta = kTick;
}

void PenumbraLayer::OnDetach(entt::registry& registry) { (void)registry; }

void PenumbraLayer::OnFixedUpdate(entt::registry& registry, float fixedDelta) {
    (void)registry;
    (void)fixedDelta;
}

void PenumbraLayer::OnUpdate(entt::registry& registry, float deltaTime) {
    (void)registry;
    (void)deltaTime;
}

} // namespace Penumbra
