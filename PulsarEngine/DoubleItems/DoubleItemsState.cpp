// Keep native roulette and activation behavior.
#include <DoubleItems/Native.hpp>
#include <DoubleItems/Boxes.hpp>
#include <core/egg/mem/Heap.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <MarioKartWii/Kart/KartStatus.hpp>

namespace Pulsar { namespace DoubleItems {
// Construct Slots in native Player::Init, not boot-time .ctors.
struct SlotStorage { u32 words[(sizeof(Slot) + sizeof(u32) - 1) / sizeof(u32)]; };
static SlotStorage storage[Config::Capacity];
static Slot* slots[Config::Capacity];
static bool raceEnabled;
static u32 pickupFrame[Config::Capacity];
static bool hasPickupFrame[Config::Capacity];
bool PickupCooldownActive(unsigned id) {
    return Enabled() && id < Config::Capacity && hasPickupFrame[id]
        && Raceinfo::sInstance
        && Raceinfo::sInstance->raceFrames - pickupFrame[id] < Config::PickupCooldownFrames;
}
void BeginRace() {
    raceEnabled = Config::Enabled;
    memset(hasPickupFrame, 0, sizeof(hasPickupFrame));
    // Clear old Player pointers across race rebuilds and mode/count changes.
    for(u32 id = 0; id < Config::Capacity; ++id) if(slots[id]) {
        Slot& slot = *slots[id];
        slot.owner = nullptr;
        slot.occupied = slot.ready = slot.mutePromotion = false;
        slot.roulette.itemPlayer = nullptr;
        slot.roulette.isTheRouletteSpinning = 0;
        slot.display.itemPlayer = nullptr;
    }
}
bool Enabled() {
    if(!raceEnabled || !Racedata::sInstance || DriverMgr::isOnlineRace) return false;
    const RacedataScenario& s = Racedata::sInstance->racesScenario;
    return (s.settings.gametype == GAMETYPE_DEFAULT || s.settings.gametype == GAMETYPE_CPU_RACE)
        && (s.settings.gamemode == MODE_VS_RACE || s.settings.gamemode == MODE_GRAND_PRIX
            || s.settings.gamemode == MODE_BATTLE)
        && s.localPlayerCount >= 1 && s.localPlayerCount <= 4
        && s.playerCount <= Config::Capacity;
}
Slot* Find(Item::Player& player) {
    if(!Enabled() || player.isRemote || player.id >= Config::Capacity) return nullptr;
    Slot* s = slots[player.id];
    if(s && s->owner == &player) return s;
    return nullptr;
}
void Reset(Slot& s, Item::Player* owner) {
    s.owner = owner;
    s.occupied = s.ready = s.mutePromotion = false;
    DIRouletteInit(&s.roulette, owner);
    s.display.itemPlayer = owner;
    s.display.ClearAll();
}
static void InitPlayer(Item::Player* player, u8 id) {
    if(id == 0) BeginRace();
    DIPlayerInit(player, id);
    if(!Enabled()) return;
    if(id < Config::Capacity) {
        if(!slots[id]) slots[id] = new(static_cast<void*>(&storage[id])) Slot;
        Reset(*slots[id], player);
        hasPickupFrame[id] = false;
    }
}
// Swap the inventory and roulette while reading the reserve slot; keep the real Player and Manager.
static SlotView* pickupView = nullptr;
bool IsReservePickup() { return pickupView != nullptr; }
SlotView::SlotView(Item::Player& p, const Slot& s, bool hud)
    : player(p), roulette(p.roulette), inventory(p.inventory), previous(pickupView), pickup(!hud), primaryUseFeedback(p.unknown_0x238[0]) {
    if(pickup) pickupView = this;
    p.roulette = s.roulette;
    p.inventory = s.display;
    // Player+0x238 pulses when using a Golden Mushroom. Do not show that primary-slot pulse on the reserve HUD.
    if(hud) p.unknown_0x238[0] = 0;
    if(hud && s.ready) p.roulette.isTheRouletteSpinning = 0;
}
SlotView::SlotView(Item::Player& p, ItemId item, u32 count)
    : player(p), roulette(p.roulette), inventory(p.inventory), previous(pickupView), pickup(false), primaryUseFeedback(p.unknown_0x238[0]) {
    // Show dragged items without calling SetItem; that would spawn them twice and count both copies.
    p.inventory.currentItemId = item;
    p.inventory.currentItemCount = count;
    p.roulette.isTheRouletteSpinning = 0;
}
SlotView::~SlotView() {
    player.roulette = roulette; player.inventory = inventory;
    player.unknown_0x238[0] = primaryUseFeedback;
    if(pickup) pickupView = previous;
}
int SlotView::NativeHeldCount(ItemObjId id) {
    const Item::PlayerRoulette reserve = player.roulette;
    const Item::PlayerInventory display = player.inventory;
    player.roulette = roulette; player.inventory = inventory;
    const int count = DIHeldCount(id);
    player.roulette = reserve; player.inventory = display;
    return count;
}
bool SlotView::HasTriple(Item::Player* p, u8 check) {
    if(p != &player) return DIHasTriple(p, check);
    const Item::PlayerInventory display = player.inventory;
    player.inventory = inventory;
    const bool result = DIHasTriple(p, check);
    player.inventory = display;
    return result;
}
ItemId PrimaryItem(const Item::Player& p, u32& count) {
    // Keep the slot occupied while Bullet is active, even if native item flags have cleared.
    if(p.kartPlayer->pointers.kartStatus->bitfield1 & 0x08000000) {
        count = 1;
        return BULLET_BILL;
    }
    count = p.inventory.currentItemCount;
    if(p.inventory.currentItemId != ITEM_NONE) return p.inventory.currentItemId;
    const Item::PlayerObj& objects = p.playerObj;
    // UseItem gives the attached items to PlayerObj (+0x18/+0x50); the last thrown item clears those fields.
    if(objects.useType >= Item::PlayerObj::TRAIL && objects.useType <= Item::PlayerObj::SPIN
        && objects.activeItemCount) {
        count = objects.activeItemCount;
        return objects.itemId;
    }
    // Ignore an old roulette result once its active bit has cleared.
    if((p.bitfield & 0x1000) && (p.roulette.unknown_0x24 >= 0 && p.roulette.unknown_0x24 < 19)) {
        count = 1;
        return static_cast<ItemId>(p.roulette.unknown_0x24);
    }
    count = 0;
    return ITEM_NONE;
}
bool PrimaryBusy(const Item::Player& p) {
    u32 count;
    return PrimaryItem(p, count) != ITEM_NONE || p.roulette.isTheRouletteSpinning
        || (p.bitfield & 0x1100);
}
bool HasInventory(Item::Player& p) {
    const Slot* s = Find(p);
    return PrimaryBusy(p) || (s && s->occupied);
}
static bool CollectOne(Item::Player* p, u16 human, u16 cpu, u32 lottery) {
    Slot* s = Find(*p);
    if(!s) { DIDecide(p, human, cpu, lottery); return false; }
    if(!PrimaryBusy(*p) && !s->occupied) {
        DIDecide(p, human, cpu, lottery);
        return p->roulette.isTheRouletteSpinning != 0 || p->inventory.currentItemId != ITEM_NONE;
    }
    if(s->occupied) return false;
    // Keep native finish-state, box setting, CPU/human and position checks.
    {
        SlotView view(*p, *s, false);
        DIDecide(p, human, cpu, lottery);
        s->roulette = p->roulette;
    }
    s->occupied = s->roulette.isTheRouletteSpinning != 0;
    return s->occupied;
}
static void Pickup(Item::Player* p, u16 human, u16 cpu, u32 lottery) {
    // Use the saved box choice; native finish/settings checks still apply and full slots stop grants.
    const unsigned rewards = BoxRewardCount();
    bool collected = false;
    for(unsigned i = 0; i < rewards; ++i) {
        if(CollectOne(p, human, cpu, lottery)) collected = true;
    }
    // Start the cooldown after both items are granted. Damage and slot promotion must not reset it.
    if(collected && p->id < Config::Capacity && Raceinfo::sInstance) {
        pickupFrame[p->id] = Raceinfo::sInstance->raceFrames;
        hasPickupFrame[p->id] = true;
    }
}
static void Promote(Item::Player& p, Slot& s) {
    if(!s.occupied || PrimaryBusy(p) || p.inventory.loseDelayDueToDmg) return;
    p.roulette = s.roulette;
    p.roulette.itemPlayer = &p;
    if(s.ready) {
        // Finish the existing roll so Thunder Cloud activation and pickup sounds still run; do not roll again.
        p.roulette.isTheRouletteSpinning = 2;
        p.roulette.nextRandomItem = p.roulette.nextItemId;
        p.roulette.unknown_0x10[1] = 1.0f;
    }
    const bool completed = s.ready;
    Reset(s, &p);
    s.mutePromotion = completed;
}
// Let the reserve roulette play its normal sounds.
// Mute only the duplicate acquisition sound when promoting a ready item.
static u32 mutedHud = 0xffffffff;
static bool AcquireSound(Audio::RSARPlayer* audio, u32 sound, u8 hud) {
    if(sound == 0xe3 && hud == mutedHud) return false;
    return audio->PlaySound(sound, hud);
}
kmCall(0x80798160, AcquireSound);
static void UpdatePlayer(Item::Player* p) {
    Slot* s = Find(*p);
    if(s) {
        const Kart::Status* status = p->kartPlayer->pointers.kartStatus;
        const bool finished = (Raceinfo::sInstance->players[p->id]->stateFlags & 2) != 0;
        if(finished || (status->bitfield0 & 0x10) || (status->bitfield1 & 2)) Reset(*s, p);
        else Promote(*p, *s);
    }
    const u32 previousMutedHud = mutedHud;
    mutedHud = s && s->mutePromotion ? p->hudSlotId : 0xffffffff;
    p->Update();
    mutedHud = previousMutedHud;
    if(!s) return;
    s->mutePromotion = false;
    if(s->occupied && !s->ready && p->hudSlotId < 4)
        DIHoldRouletteSound(Audio::RSARPlayer::sInstance, p->hudSlotId);
    if(s->occupied && !s->ready && DIRouletteUpdate(&s->roulette)) {
        s->ready = true;
        s->display.SetItem(s->roulette.nextItemId, s->roulette.isItemForcedDueToCapacity);
        if(p->hudSlotId < 4) Audio::RSARPlayer::sInstance->PlaySound(0xe3, p->hudSlotId);
    }
    Promote(*p, *s);
}
static void Lose(Item::Player* p, u32 index) {
    Slot* s = Find(*p);
    // Clear the reserve on damage even if the primary slot is empty or holds a dragged item.
    // Cancel a spinning reserve roulette, but keep an acquired Thunder Cloud (ID 14) as the game does.
    if(s && (!s->ready || s->display.currentItemId != THUNDER_CLOUD)) {
        if(s->ready) s->display.LoseItemFromDmg();
        Reset(*s, p);
    }
    DILose(p, index);
}
static int HeldCount(ItemObjId id) {
    int count = pickupView ? pickupView->NativeHeldCount(id) : DIHeldCount(id);
    if(!Enabled() || !Item::Manager::sInstance) return count;
    Item::Manager& mgr = *Item::Manager::sInstance;
    for(u32 i = 0; i < mgr.playerCount && i < Config::Capacity; ++i) {
        if(!slots[i]) continue;
        const Slot& s = *slots[i];
        if(s.owner != &mgr.players[i] || !s.occupied || s.roulette.isItemForcedDueToCapacity) continue;
        const ItemId item = s.roulette.nextItemId;
        if((item >= 0 && item < 19) && Item::Behavior::behaviourTable[item].objId == id)
            count += Item::Behavior::behaviourTable[item].numberOfItems;
    }
    return count;
}
static bool HasTriple(Item::Player* p, u8 check) {
    if(Find(*p) && check == 1 && p->playerObj.activeItemCount
        && (p->playerObj.useType == Item::PlayerObj::TRIPLE_TRAIL
            || p->playerObj.useType == Item::PlayerObj::SPIN)) return true;
    return pickupView ? pickupView->HasTriple(p, check) : DIHasTriple(p, check);
}
kmCall(0x807ba0cc, HasTriple);
kmCall(0x807993dc, InitPlayer);
kmCall(0x80828d70, Pickup);
kmCall(0x80828da4, Pickup);
kmCall(0x8079994c, UpdatePlayer);
kmCall(0x80799afc, UpdatePlayer);
kmCall(0x807964f8, HeldCount);
kmCall(0x80796580, HeldCount);
kmCall(0x80797508, HeldCount);
kmCall(0x80568168, Lose);
kmCall(0x80568778, Lose);
kmCall(0x80568d18, Lose);
kmCall(0x80569084, Lose);
kmCall(0x805691e8, Lose);
kmCall(0x8056973c, Lose);
kmCall(0x805697e4, Lose);
kmCall(0x80569888, Lose);
kmCall(0x8056992c, Lose);
kmCall(0x805699e8, Lose);
kmCall(0x805808ec, Lose);
} }
