#ifndef PUL_DOUBLE_ITEMS_NATIVE
#define PUL_DOUBLE_ITEMS_NATIVE
#include <DoubleItems/State.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceItemWindow.hpp>
extern "C" {
void DIPlayerInit(Item::Player*, u8);
void DIDecide(Item::Player*, u16, u16, u32);
void DILose(Item::Player*, u32);
void DIRouletteInit(Item::PlayerRoulette*, Item::Player*);
bool DIRouletteUpdate(Item::PlayerRoulette*);
int DIHeldCount(ItemObjId);
bool DIHasTriple(Item::Player*, u8);
void DIWindowLoad(CtrlRaceItemWindow*, const char*, u8);
void DIWindowUpdate(CtrlRaceItemWindow*);
bool DIWindowInactive(CtrlRaceItemWindow*);
void DIResetPosition(UIControl*);
void DIHoldRouletteSound(Audio::RSARPlayer*, s8);
}
#endif
