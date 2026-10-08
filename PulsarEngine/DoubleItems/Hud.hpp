#ifndef PUL_DOUBLE_ITEMS_HUD
#define PUL_DOUBLE_ITEMS_HUD
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceItemWindow.hpp>
namespace Pulsar { namespace DoubleItems {
class QueueWindow : public CtrlRaceItemWindow {
    bool inventoryVisible;
public:
    QueueWindow() : inventoryVisible(false) {}
    ~QueueWindow() override;
    void Init() override;
    void Draw(u32 zIndex) override;
    bool IsInactive() override;
    bool HasStarted() override;
    void OnUpdate() override;
};
} }
#endif
