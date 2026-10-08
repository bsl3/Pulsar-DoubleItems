#ifndef PUL_DOUBLE_ITEMS_STATE
#define PUL_DOUBLE_ITEMS_STATE
#include <DoubleItems/DoubleItems.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
namespace Pulsar { namespace DoubleItems {
struct Slot {
    Item::Player* owner;
    Item::PlayerRoulette roulette;
    Item::PlayerInventory display;
    bool occupied, ready, mutePromotion;
};
size_assert(Slot, 0x68);
Slot* Find(Item::Player& player);
void Reset(Slot& slot, Item::Player* owner);
// Attached items belong to PlayerObj but still occupy the primary slot.
ItemId PrimaryItem(const Item::Player& player, u32& count);
bool PrimaryBusy(const Item::Player& player);
bool HasInventory(Item::Player& player);
// Used by diagnostics to check whether a pickup is reading the reserve slot.
bool IsReservePickup();
// Temporarily show the reserve inventory and roulette; restore the primary slot when this scope ends.
class SlotView {
    Item::Player& player;
    Item::PlayerRoulette roulette;
    Item::PlayerInventory inventory;
    SlotView* previous;
    bool pickup;
    u8 primaryUseFeedback;
    SlotView(const SlotView&);
    SlotView& operator=(const SlotView&);
public:
    SlotView(Item::Player& player, const Slot& slot, bool hud);
    SlotView(Item::Player& player, ItemId item, u32 count);
    ~SlotView();
    int NativeHeldCount(ItemObjId id);
    bool HasTriple(Item::Player* player, u8 check);
};
} }
#endif
