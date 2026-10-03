#pragma once

// Persistent user settings (pure C++ apart from the toolset's flash store
// interface, which has no SDK dependency: host-testable, see tests/).

#include "pico_toolset/flash_store.h"

#include <cstddef>
#include <cstdint>

/// @brief What survives a reboot. Add fields at the END and bump kVersion's
/// handling in deserialize() (old records must stay readable or be ignored).
struct Settings {
    uint8_t backlight = 255;  //! user backlight level (0-255)
    uint8_t page = 0;         //! Pages::Id of the page to show

    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kSize = 3;   // version, backlight, page

    bool operator==(const Settings& o) const { return backlight == o.backlight && page == o.page; }
    bool operator!=(const Settings& o) const { return !(*this == o); }

    size_t serialize(uint8_t* out) const;
    //! false (and `out` untouched) for a wrong version or a short record
    static bool deserialize(const uint8_t* data, size_t len, Settings& out);
};

/// @brief Settings in a pico_toolset::FlashStore
class SettingsStore {
public:
    explicit SettingsStore(pico_toolset::FlashStore& store) : m_store(store) {}

    //! @return true if a valid record was loaded into `out` (else `out` is unchanged)
    bool load(Settings& out) const;
    bool save(const Settings& s);

private:
    pico_toolset::FlashStore& m_store;
};

/// @brief Saves settings only after they have stopped changing: flash writes
/// briefly halt the firmware, and e.g. holding a brightness corner changes the
/// value many times a second. Call update() once per main-loop iteration.
class SettingsSaver {
public:
    //! The value must stay unchanged this long before it is written. Long on
    //! purpose (fewer flash writes); a change made less than this long before
    //! power-off is lost.
    static constexpr uint32_t kSettleMs = 30000;

    using SaveFn = bool (*)(void* context, const Settings&);

    //! @param saved the settings currently stored (what was loaded at boot)
    SettingsSaver(const Settings& saved, SaveFn save, void* context)
        : m_saved(saved), m_pending(saved), m_save(save), m_context(context) {}

    void update(const Settings& wanted, uint32_t now_ms);

private:
    Settings m_saved;
    Settings m_pending;
    bool m_has_pending = false;
    uint32_t m_since_ms = 0;
    SaveFn m_save;
    void* m_context;
};
