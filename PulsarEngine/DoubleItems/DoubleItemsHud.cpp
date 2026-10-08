#include <DoubleItems/Hud.hpp>
#include <DoubleItems/Native.hpp>
#include <UI/CtrlRaceBase/CustomCtrlRaceBase.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>
#include <core/nw4r/lyt/Material.hpp>

namespace Pulsar { namespace DoubleItems {
// Keep Pulsar's icon atlas/variants, but use the original frame hierarchy.
// The old IKW item_vindow contains independently transformed bonus/time borders.
static bool loadingWindow;
static void BuildWindowFrame(MainLayout* layout, const char* name) {
    layout->Build(loadingWindow ? "item_window" : name);
}
kmCall(0x805c2d4c, BuildWindowFrame);
static void LoadWindow(CtrlRaceItemWindow* window, const char* variant, u8 hud) {
    const bool previous = loadingWindow;
    loadingWindow = Enabled();
    DIWindowLoad(window, variant, hud);
    loadingWindow = previous;
}
static QueueWindow* windows[4];
// Native highlight materials have no alpha test. Their transparent margins
// still write depth and can cut holes in the neighbouring rotating window.
// Discard transparent pixels on these two borders so they do not hide the other item window.
// Keep the game's filtering, transforms and depth sorting.
static CtrlRaceItemWindow* drawingWindow;
extern "C" bool DISetupWindowMaterial(nw4r::lyt::Material* material, bool modVtxColor, u8 alpha, nw4r::lyt::Pane* pane) {
    const bool result = material->SetupGX(modVtxColor, alpha);
    if(drawingWindow && (material == drawingWindow->hilight_curr->material
        || material == drawingWindow->hilight_next->material)) {
        GX::SetAlphaCompare(GX::GX_GREATER, 0, GX::GX_AOP_AND, GX::GX_ALWAYS, 0);
    }
    return result;
}
asmFunc DISetupWindowMaterialCall() { ASM(nofralloc;
    // Picture::DrawSelf keeps the actual picture in r28 at its SetupGX call.
    // Keep the material, color and alpha arguments; use r28 to identify the face being drawn.
    mr r6,r28;
    b DISetupWindowMaterial;
) }
kmCall(0x8007b284, DISetupWindowMaterialCall);
static void DrawWindow(CtrlRaceItemWindow* window, u32 zIndex) {
    CtrlRaceItemWindow* previous = drawingWindow;
    drawingWindow = nullptr;
    if(Enabled()) drawingWindow = window;
    window->LayoutUIControl::Draw(zIndex);
    drawingWindow = previous;
}
void QueueWindow::Draw(u32 zIndex) { DrawWindow(this, zIndex); }
kmWritePointer(0x808d3cdc, DrawWindow);
struct FacePose {
    // Store plain bytes here so no native vector constructor runs before StaticR loads.
    float trans[3], rotate[3];
    void Capture(const nw4r::lyt::Pane& pane) {
        memcpy(trans, &pane.trans, sizeof(trans));
        memcpy(rotate, &pane.rotate, sizeof(rotate));
    }
    void Restore(nw4r::lyt::Pane& pane) const {
        memcpy(&pane.trans, trans, sizeof(trans));
        memcpy(&pane.rotate, rotate, sizeof(rotate));
    }
};
struct WindowPose {
    CtrlRaceItemWindow* window;
    FacePose current, next;
};
static WindowPose poses[8]; // two native controls per local HUD; recaptured on Init
static void InitWindow(CtrlRaceItemWindow* window) {
    window->CtrlRaceItemWindow::Init();
    if(!Enabled()) return;
    // Save both faces' original transforms, including the hidden rear face.
    // Setting both transforms to identity would put them on the same plane.
    const u32 index = window->hudSlotId * 2 + (window == windows[window->hudSlotId] ? 1 : 0);
    poses[index].window = window;
    poses[index].current.Capture(*window->item_curr_null);
    poses[index].next.Capture(*window->item_next_null);
    window->framesSinceLastSpin = 9; // no acquire-item flash on an empty frame

}
// The roulette rotates two faces. When it stops, restore the whole resting transform;
// resetting only the glass scale and alpha leaves the reserve frame tilted or out of place.
static void SetRestingFace(nw4r::lyt::Pane* face, const FacePose& pose, bool visible) {
    if(visible) face->flag |= 1;
    else face->flag &= ~1;
    pose.Restore(*face);
}
static void UpdateWindow(CtrlRaceItemWindow* window, const Item::Player& player) {
    window->item_curr->flag |= 1;
    window->item_next->flag |= 1;
    DIWindowUpdate(window);
    if(!player.roulette.isTheRouletteSpinning) {
        const u32 index = window->hudSlotId * 2 + (window == windows[window->hudSlotId] ? 1 : 0);
        const WindowPose& pose = poses[index];
        SetRestingFace(window->item_curr_null, pose.current, false);
        SetRestingFace(window->item_next_null, pose.next, true);
        if(player.inventory.currentItemId == ITEM_NONE) {
            window->item_curr->flag &= ~1;
            window->item_next->flag &= ~1;
        }
    }
}

QueueWindow::~QueueWindow() {
    if(hudSlotId < 4 && windows[hudSlotId] == this) windows[hudSlotId] = nullptr;
}
void QueueWindow::Init() { InitWindow(this); }
bool QueueWindow::HasStarted() {
    return inventoryVisible && CtrlRaceBase::HasStarted();
}
bool QueueWindow::IsInactive() {
    // HasStarted starts the HUD animation. IsInactive must end it whenever the slot becomes empty.
    return !inventoryVisible || CtrlRaceBase::IsInactive();
}
void QueueWindow::OnUpdate() {
    Item::Manager* mgr = Item::Manager::sInstance;
    const u32 id = GetPlayerId();
    if(!mgr || id >= mgr->playerCount) return;
    Item::Player& player = mgr->players[id];
    const Slot* s = Find(player);
    if(!s) return;
    // Check both slots before switching to the reserve view; both HUD controls need the same answer.
    inventoryVisible = HasInventory(player);
    SlotView view(player, *s, true);
    UpdateWindow(this, player);
}
static u32 Count() {
    for(u32 i = 0; i < 4; ++i) windows[i] = nullptr;
    return Enabled() ? Racedata::sInstance->racesScenario.localPlayerCount : 0;
}
static void Create(Page& page, u32 index, u32 count) {
    for(u32 i = 0; i < count; ++i) {
        QueueWindow* window = new QueueWindow;
        window->hudSlotId = i;
        windows[i] = window;
        page.AddControl(index + i, *window, 0);
        // The primary Load hook sets the split-screen variant later in this page's initialization,
        // before ControlGroup::Init.
    }
}
static void LoadPrimary(CtrlRaceItemWindow* primary, const char* variant, u8 hud) {
    LoadWindow(primary, variant, hud);
    if(!Enabled() || hud >= 4 || !windows[hud]) return;
    QueueWindow& window = *windows[hud];
    LoadWindow(&window, variant, hud);
    PositionAndScale& base = window.positionAndscale[0];
    const float offset = Racedata::sInstance->racesScenario.localPlayerCount == 1
        ? Config::SingleOffsetX : Config::MultiOffsetX;
    base.position.x += base.position.x > 0.0f ? -offset : offset;
    base.position.y += Config::OffsetY;
    base.scale.x *= Config::QueueScale;
    base.scale.z *= Config::QueueScale;
    DIResetPosition(&window); // rebuild cached draw transforms after Load
}
static void PrimaryUpdate(CtrlRaceItemWindow* window) {
    if(!Enabled()) {
        DIWindowUpdate(window);
        Item::Manager* manager=Item::Manager::sInstance;
        const u32 id=window->GetPlayerId();
        return;
    }
    Item::Player& player = Item::Manager::sInstance->players[window->GetPlayerId()];
    u32 count;
    const ItemId item = PrimaryItem(player, count);
    if(player.inventory.currentItemId == ITEM_NONE && !player.roulette.isTheRouletteSpinning
        && item != ITEM_NONE) {
        SlotView held(player, item, count);
        UpdateWindow(window, player);
    }
    else UpdateWindow(window, player);
}
static bool PrimaryStarted(CtrlRaceItemWindow* window) {
    if(!Enabled()) return window->CtrlRaceItemWindow::HasStarted();
    Item::Manager* mgr = Item::Manager::sInstance;
    const u32 id = window->GetPlayerId();
    return mgr && id < mgr->playerCount && HasInventory(mgr->players[id])
        && window->CtrlRaceBase::HasStarted();
}
static bool PrimaryInactive(CtrlRaceItemWindow* window) {
    if(!Enabled()) return DIWindowInactive(window);
    Item::Manager* mgr = Item::Manager::sInstance;
    const u32 id = window->GetPlayerId();
    return !mgr || id >= mgr->playerCount || !HasInventory(mgr->players[id])
        || window->CtrlRaceBase::IsInactive();
}
static UI::CustomCtrlBuilder builder(Count, Create);
kmCall(0x80857ed4, LoadPrimary);
kmWritePointer(0x808d3d10, PrimaryInactive);
kmWritePointer(0x808d3ce4, PrimaryUpdate);
kmWritePointer(0x808d3cd4, InitWindow);
kmWritePointer(0x808d3d14, PrimaryStarted);
} }
