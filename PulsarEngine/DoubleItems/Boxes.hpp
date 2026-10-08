#ifndef PUL_DOUBLE_ITEMS_BOXES
#define PUL_DOUBLE_ITEMS_BOXES
namespace Pulsar { namespace DoubleItems {
// Read the current box collision; do not roll the double-box chance again.
unsigned BoxRewardCount();
// Check the cooldown for regular boxes; a double box grants both items together.
bool PickupCooldownActive(unsigned playerId);
} }
#endif
