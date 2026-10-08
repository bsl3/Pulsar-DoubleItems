#include <DoubleItems/DoubleItems.hpp>
#include <DoubleItems/Boxes.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/Objects/Collidable/ObjectCollidable.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <core/egg/mem/Disposer.hpp>
#include <core/egg/mem/Heap.hpp>
extern "C" {
void DIBindBoxBRRES(nw4r::g3d::ResFile*, ArchiveSource, const char*, const nw4r::g3d::ResFile*);
ObjToKartHit DIBoxCollision(Object*, const Kart::Player*, ObjToKartHit, KartToObjHit);
Random* DIBoxRandom();
ObjToKartHit DIConveyorBoxCollision(Object*, const Kart::Player*, ObjToKartHit, KartToObjHit);
ObjToKartHit DILineBoxCollision(Object*, const Kart::Player*, ObjToKartHit, KartToObjHit);
void DIBoxUpdateModel(Object*);
}
namespace Pulsar { namespace DoubleItems {
class BoxRecord;
static BoxRecord* boxes;
static const Object* collectingBox;

// The scene heap deletes this record; its destructor removes it from the list.
class BoxRecord : public EGG::Disposer {
public:
    BoxRecord* next;
    const Object* object;
    bool doubleBox;
    ModelDirector* upperModel;
    BoxRecord(const Object* obj, bool isDouble)
        : next(boxes), object(obj), doubleBox(isDouble), upperModel(nullptr) { boxes = this; }
    ~BoxRecord() override {
        // The scene heap owns both upperModel and its G3D backing.
        BoxRecord** link = &boxes;
        while(*link && *link != this) link = &(*link)->next;
        if(*link) *link = next;
    }
};
static BoxRecord* FindBox(const Object* object) {
    for(BoxRecord* record = boxes; record; record = record->next)
        if(record->object == object) return record;
    return nullptr;
}

// Keep the course BRRES and choose normal/double once per box.
static void BindBox(nw4r::g3d::ResFile* file, ArchiveSource source, const char* name,
    const nw4r::g3d::ResFile* fallback) {
    if(Enabled() && source == ARCHIVE_HOLDER_COURSE && !strcmp(name, "itembox.brres")) {
        // At this exact native call site r3 is Object::rawBrres (object + 0x14).
        const Object* object = reinterpret_cast<Object*>(reinterpret_cast<u8*>(file) - 0x14);
        if(!FindBox(object)) {
            const bool doubleBox = DIBoxRandom()->NextLimited(Config::DoubleBoxOdds) == 0;
            new BoxRecord(object, doubleBox);
        }
    }
    DIBindBoxBRRES(file, source, name, fallback);
}

static void SetModelMatrix(ModelDirector* director, const Mtx34& matrix) {
    if(!director || !director->curScnMdlEx || !director->curScnMdlEx->scnObj)
        return;

    nw4r::g3d::ScnObj* scnObj =
        reinterpret_cast<nw4r::g3d::ScnObj*>(
            director->curScnMdlEx->scnObj);

    scnObj->SetMtx(nw4r::g3d::ScnObj::MTX_LOCAL, matrix);
}

static ModelDirector* CreateUpperModel(Object& box) {
    ModelDirector* primary = box.mdlDirector;
    if(!primary || !box.rawBrres.data) return nullptr;

    // Reuse the box's BRRES for the upper model; do not create a second collidable box.
    ModelDirector* upper = new ModelDirector(primary->scnObjDrawOptionsIdx, 0);
    const char* modelName = box.GetBRRESName();
    upper->LoadNoAnm(modelName ? modelName : "itembox", box.rawBrres, primary->light);
    return upper;
}

// Run native Object::UpdateModel first, then copy its final matrix to the stacked visual.
static void UpdateBoxModel(Object* box) {
    DIBoxUpdateModel(box);

    BoxRecord* record = FindBox(box);
    if(!Enabled() || !record || !record->doubleBox || !Config::StackedNativeBoxes) return;

    if(!record->upperModel) record->upperModel = CreateUpperModel(*box);
    if(!record->upperModel || !box->mdlDirector) return;

    nw4r::g3d::ScnObj* primaryObj = 0;
    nw4r::g3d::ScnObj* upperObj = 0;

    if(box->mdlDirector->curScnMdlEx) {
        primaryObj = reinterpret_cast<nw4r::g3d::ScnObj*>(
            box->mdlDirector->curScnMdlEx->scnObj);
    }

    if(record->upperModel->curScnMdlEx) {
        upperObj = reinterpret_cast<nw4r::g3d::ScnObj*>(
            record->upperModel->curScnMdlEx->scnObj);
    }
    if(primaryObj && upperObj) {
        // Copy the native final matrix. A zero base offset leaves the lower model unchanged.
        Mtx34 lower;
        for(u32 i = 0; i < 12; ++i)
            lower.member[i] = primaryObj->matrixArray[nw4r::g3d::ScnObj::MTX_LOCAL].member[i];
        if(Config::DoubleBoxBaseVisualOffsetY != 0.0f) {
            lower.n13 += Config::DoubleBoxBaseVisualOffsetY;
            SetModelMatrix(box->mdlDirector, lower);
            if(box->mdlLodDirector) SetModelMatrix(box->mdlLodDirector, lower);
        }

        Mtx34 upper = lower;
        upper.n13 += Config::DoubleBoxStackSpacingY;
        SetModelMatrix(record->upperModel, upper);

        // Copy native ScnLeaf scale so the second box grows/shrinks with the first.
        reinterpret_cast<nw4r::g3d::ScnLeaf*>(upperObj)->scale =
            reinterpret_cast<nw4r::g3d::ScnLeaf*>(primaryObj)->scale;

        // Draw the extra model only while the native +0xb0 active flag is set.
        const bool active = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(box) + 0xb0) != 0;
        const u32 disable = active && box->isVisible ? 0 : 1;
        upperObj->SetScnObjOption(nw4r::g3d::ScnObj::OPTID_DISABLE_DRAW_OPA, disable);
        upperObj->SetScnObjOption(nw4r::g3d::ScnObj::OPTID_DISABLE_DRAW_XLU, disable);
    }
}

unsigned BoxRewardCount() {
    const BoxRecord* record = FindBox(collectingBox);
    return Enabled() && record && record->doubleBox ? 2 : 1;
}
static bool PickupBlocked(const Object* box, const Kart::Player* kart) {
    if(!kart || !Enabled()) return false;
    const BoxRecord* record = FindBox(box);
    if(record && record->doubleBox) return false;
    return PickupCooldownActive(kart->GetPlayerIdx());
}
static bool FastRespawn() {
    return Enabled() || (Racedata::sInstance && Racedata::sInstance->racesScenario.playerCount > 12);
}
static ObjToKartHit CollectBox(Object* box, const Kart::Player* kart, ObjToKartHit hit, KartToObjHit kartHit) {
    if(PickupBlocked(box, kart)) return static_cast<ObjToKartHit>(0);
    const Object* previous = collectingBox;
    collectingBox = box;
    const ObjToKartHit result = DIBoxCollision(box, kart, hit, kartHit);
    collectingBox = previous;
    // OnCollision clears +b0/+b4; Update checks +b8 at 808288b8.
    // Change only the regeneration threshold after collection.
    u32* timing = reinterpret_cast<u32*>(reinterpret_cast<u8*>(box) + 0xb0);
    if(FastRespawn() && timing[0] == 0)
        timing[2] = Config::FastRespawnFrames - 1;
    return result;
}
// Reject cooldown pickups before conveyor tails consume the box.
static ObjToKartHit CollectConveyorBox(Object* box, const Kart::Player* kart, ObjToKartHit hit, KartToObjHit kartHit) {
    if(PickupBlocked(box, kart)) return static_cast<ObjToKartHit>(0);
    return DIConveyorBoxCollision(box, kart, hit, kartHit);
}
static ObjToKartHit CollectLineBox(Object* box, const Kart::Player* kart, ObjToKartHit hit, KartToObjHit kartHit) {
    if(PickupBlocked(box, kart)) return static_cast<ObjToKartHit>(0);
    return DILineBoxCollision(box, kart, hit, kartHit);
}

kmWritePointer(0x808ced38, CollectConveyorBox);
kmWritePointer(0x808cef60, CollectLineBox);
kmCall(0x8081fdb4, BindBox);
// Native base collision is shared by stationary, moving and floating boxes.
kmWritePointer(0x808d7c80, CollectBox); // Itembox
kmWritePointer(0x808d7b90, CollectBox); // F_Itembox
kmWritePointer(0x808d7aa0, CollectBox); // S_Itembox
kmWritePointer(0x808d0f90, CollectBox); // Sin_Itembox
// Conveyor subclasses call the base collision directly; retain their own tail.
kmCall(0x8076cf10, CollectBox);
kmCall(0x8076e0d0, CollectBox);

// All six vtable slots originally use Object::UpdateModel (808217b8).
// Patch each subtype so its second model stays attached.
kmWritePointer(0x808d7bdc, UpdateBoxModel); // Itembox
kmWritePointer(0x808d7aec, UpdateBoxModel); // F_Itembox
kmWritePointer(0x808d79fc, UpdateBoxModel); // S_Itembox
kmWritePointer(0x808d0eec, UpdateBoxModel); // Sin_Itembox
kmWritePointer(0x808cec94, UpdateBoxModel); // W_Itembox::Box
kmWritePointer(0x808ceebc, UpdateBoxModel); // W_Itemboxline::Box
} }
