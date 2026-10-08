#ifndef PUL_DOUBLE_ITEMS
#define PUL_DOUBLE_ITEMS
#include <DoubleItems/Config.hpp>
namespace Pulsar { namespace DoubleItems {
// Check the offline mode and config without calling StaticR functions during boot.
bool Enabled();
// Save the menu choice and clear the reserve slots before rebuilding the race.
void BeginRace();
} }
#endif
