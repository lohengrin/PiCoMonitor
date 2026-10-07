#include "Settings.h"

size_t Settings::serialize(uint8_t* out) const
{
    out[0] = kVersion;
    out[1] = backlight;
    out[2] = page;
    out[3] = cycle;
    return kSize;
}

bool Settings::deserialize(const uint8_t* data, size_t len, Settings& out)
{
    if (len >= 3 && data[0] == 1) {         // before the cycling mode existed
        out.backlight = data[1];
        out.page = data[2];
        out.cycle = 0;
        return true;
    }
    if (len < kSize || data[0] != kVersion)
        return false;
    out.backlight = data[1];
    out.page = data[2];
    out.cycle = data[3];
    return true;
}

bool SettingsStore::load(Settings& out) const
{
    uint8_t buf[pico_toolset::FlashStore::kMaxPayload];
    size_t len = 0;
    return m_store.load(buf, len) && Settings::deserialize(buf, len, out);
}

bool SettingsStore::save(const Settings& s)
{
    uint8_t buf[Settings::kSize];
    return m_store.save({buf, s.serialize(buf)});
}

void SettingsSaver::update(const Settings& wanted, uint32_t now_ms)
{
    if (wanted == m_saved) {            // nothing to store (also: reverted to the stored value)
        m_has_pending = false;
        return;
    }
    if (!m_has_pending || wanted != m_pending) {   // (re)start the settle timer
        m_pending = wanted;
        m_has_pending = true;
        m_since_ms = now_ms;
        return;
    }
    if (now_ms - m_since_ms < kSettleMs)
        return;

    if (m_save(m_context, wanted)) {
        m_saved = wanted;
        m_has_pending = false;
    } else {
        m_since_ms = now_ms;            // failed: try again after another settle period
    }
}
