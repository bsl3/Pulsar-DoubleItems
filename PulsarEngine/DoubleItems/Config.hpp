#ifndef PUL_DOUBLE_ITEMS_CONFIG
#define PUL_DOUBLE_ITEMS_CONFIG
namespace Pulsar { namespace DoubleItems { namespace Config {
static const bool Enabled = true;
// Draw the course's item-box model twice for a double box.
// Only the original box handles collisions and pickups.
static const bool StackedNativeBoxes = true;
// Native race simulation ticks (60 Hz), independent of the render FPS.
static const unsigned FastRespawnFrames = 12; // 0.2 seconds
static const unsigned PickupCooldownFrames = 120; // 2 seconds
static const unsigned Capacity = 24;
static const int DoubleBoxOdds = 10; // one in 10 chance (10%), sampled once per box instance
static const float QueueScale = 0.80f;
static const float SingleOffsetX = 84.0f;
static const float MultiOffsetX = 62.0f;
static const float OffsetY = 0.0f;
// These Y offsets move the models, not the collision. BaseVisualOffsetY moves both boxes;
// leave it at zero to keep the lower model at the original position.

// Raises BOTH visible boxes together
static const float DoubleBoxBaseVisualOffsetY = 0.0f;

// Distance between the bottom and top visible boxes
static const float DoubleBoxStackSpacingY = 250.0f;
} } }
#endif
