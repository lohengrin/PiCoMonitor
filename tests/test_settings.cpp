#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <vector>

using pico_toolset::FlashIo;
using pico_toolset::FlashStore;
using pico_toolset::FlashStoreConfig;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// ---- fake flash: 4 sectors, erase -> 0xFF, program may only clear bits ------------------------
namespace fake {
std::vector<uint8_t> mem;
int erases = 0, programs = 0;
bool fail_erase = false;
bool fail_program = false;      // fails before writing anything
size_t tear_bytes = 0;          // >0: only the first N bytes of the page are written, then failure (power loss)
bool bad_program_rule = false;  // set when something programs a non-erased page

void reset(size_t sectors = 2) { mem.assign(sectors * 4096, 0xFF); erases = programs = 0; fail_erase = fail_program = bad_program_rule = false; tear_bytes = 0; }
const uint8_t* read(uint32_t off) { return mem.data() + off; }
bool erase(uint32_t off) {
    if (fail_erase) return false;
    std::memset(mem.data() + off, 0xFF, 4096); erases++; return true;
}
bool program(uint32_t off, const uint8_t* page) {
    if (fail_program) return false;
    size_t n = tear_bytes ? tear_bytes : 256;
    for (size_t i = 0; i < n; ++i) {
        if ((mem[off + i] & page[i]) != page[i]) bad_program_rule = true;   // would need a 0->1 flip
        mem[off + i] &= page[i];
    }
    programs++;
    return tear_bytes == 0;
}
FlashIo io() { return FlashIo{read, erase, program}; }
}

static FlashStore open(size_t sectors = 2) {
    FlashStore s;
    s.init(FlashStoreConfig{0, static_cast<uint8_t>(sectors)}, fake::io());
    return s;
}

static bool load(const FlashStore& s, std::vector<uint8_t>& out) {
    uint8_t buf[FlashStore::kMaxPayload]; size_t len = 0;
    if (!s.load(buf, len)) return false;
    out.assign(buf, buf + len);
    return true;
}

static std::vector<uint8_t> bytes(std::initializer_list<uint8_t> v) { return v; }

static void test_basic() {
    fake::reset();
    FlashStore s = open();
    std::vector<uint8_t> out;
    CHECK(!s.has_record() && !load(s, out));

    auto a = bytes({1, 2, 3});
    CHECK(s.save(a));
    CHECK(s.has_record() && s.sequence() == 1);
    CHECK(load(s, out) && out == a);
    CHECK(fake::erases == 0 && fake::programs == 1);     // fresh flash is already erased: no erase needed

    // identical data: nothing written
    CHECK(s.save(a));
    CHECK(fake::programs == 1 && s.sequence() == 1);

    // survives a "reboot" (new FlashStore over the same memory)
    FlashStore again = open();
    CHECK(again.sequence() == 1 && load(again, out) && out == a);

    // too large
    std::vector<uint8_t> big(FlashStore::kMaxPayload + 1, 7);
    CHECK(!s.save(big));
    std::vector<uint8_t> max(FlashStore::kMaxPayload, 9);
    CHECK(s.save(max) && load(s, out) && out == max);
    CHECK(!fake::bad_program_rule);
}

static void test_rotation_and_wear() {
    fake::reset();
    FlashStore s = open();
    std::vector<uint8_t> out;
    for (int i = 1; i <= 100; ++i) {
        CHECK(s.save(bytes({static_cast<uint8_t>(i), 0})));
        CHECK(load(s, out) && out[0] == i);
    }
    CHECK(s.sequence() == 100);
    // 16 records per sector: 100 saves need only ~6 erases, not 100
    CHECK(fake::erases >= 5 && fake::erases <= 7);
    CHECK(!fake::bad_program_rule);

    FlashStore again = open();
    CHECK(again.sequence() == 100 && load(again, out) && out[0] == 100);
    // and it keeps going after the reboot, in the right place
    CHECK(again.save(bytes({200, 0})) && load(again, out) && out[0] == 200 && again.sequence() == 101);
    FlashStore third = open();
    CHECK(third.sequence() == 101 && load(third, out) && out[0] == 200);
}

static void test_corruption_and_power_loss() {
    fake::reset();
    FlashStore s = open();
    std::vector<uint8_t> out;
    CHECK(s.save(bytes({1})));
    CHECK(s.save(bytes({2})));

    // corrupt the newest record (page 1): the previous settings come back
    fake::mem[256 + 16] ^= 0x55;   // payload byte (covered by the CRC)
    FlashStore c = open();
    CHECK(c.sequence() == 1 && load(c, out) && out == bytes({1}));
    // and saving again works (the damaged page is skipped, never rewritten)
    CHECK(c.save(bytes({3})) && load(c, out) && out == bytes({3}));
    CHECK(!fake::bad_program_rule);

    // torn program (power loss mid-write): save reports failure, old data is intact
    fake::reset();
    FlashStore t = open();
    CHECK(t.save(bytes({10})));
    fake::tear_bytes = 12;                              // power lost before the checksum was written
    CHECK(!t.save(bytes({11})));
    fake::tear_bytes = 0;
    FlashStore r = open();
    CHECK(load(r, out) && out == bytes({10}));          // still the previous settings
    CHECK(r.save(bytes({12})) && load(r, out) && out == bytes({12}));   // the torn page is skipped
    CHECK(!fake::bad_program_rule);

    // torn after the whole record was written: save() reports failure but the
    // record is complete, so the new value is what comes back after a reboot
    fake::reset();
    FlashStore u = open();
    CHECK(u.save(bytes({10})));
    fake::tear_bytes = 128;
    CHECK(!u.save(bytes({11})));
    fake::tear_bytes = 0;
    FlashStore v = open();
    CHECK(load(v, out) && out == bytes({11}) && v.sequence() == 2);
    CHECK(v.save(bytes({13})) && load(v, out) && out == bytes({13}));

    // program / erase failures leave the old record in place
    fake::reset();
    FlashStore f = open();
    CHECK(f.save(bytes({20})));
    fake::fail_program = true;
    CHECK(!f.save(bytes({21})) && load(f, out) && out == bytes({20}));
    fake::fail_program = false;
    for (int i = 0; i < 40; ++i) f.save(bytes({static_cast<uint8_t>(30 + i)}));   // force sector rotations
    fake::fail_erase = true;
    int before = static_cast<int>(f.sequence());
    bool saved_any = false;
    for (int i = 0; i < 40; ++i) saved_any |= f.save(bytes({static_cast<uint8_t>(100 + i)}));
    CHECK(static_cast<int>(f.sequence()) >= before);   // never goes backwards
    fake::fail_erase = false;
    CHECK(f.save(bytes({250})) && load(f, out) && out == bytes({250}));
    (void)saved_any;

    // garbage in the region (never written by us): no record, and saving works
    fake::reset();
    for (size_t i = 0; i < fake::mem.size(); ++i) fake::mem[i] = static_cast<uint8_t>(i * 31 + 7);
    FlashStore g = open();
    CHECK(!g.has_record() && !load(g, out));
    CHECK(g.save(bytes({5})) && load(g, out) && out == bytes({5}));
    FlashStore g2 = open();
    CHECK(g2.has_record() && load(g2, out) && out == bytes({5}));
}

static void test_settings() {
    Settings s; s.backlight = 120; s.page = 2; s.cycle = 30;
    uint8_t buf[8];
    CHECK(s.serialize(buf) == Settings::kSize);
    Settings d; CHECK(Settings::deserialize(buf, Settings::kSize, d) && d == s);
    Settings x; x.backlight = 1;
    buf[0] = 99;                                      // unknown version: ignored
    CHECK(!Settings::deserialize(buf, Settings::kSize, x) && x.backlight == 1);
    buf[0] = Settings::kVersion;
    CHECK(!Settings::deserialize(buf, 3, x));         // short record
    // a record from before the cycling mode (version 1) is still read, cycling off
    { const uint8_t v1[3] = {1, 77, 3}; Settings o; o.cycle = 9;
      CHECK(Settings::deserialize(v1, 3, o) && o.backlight == 77 && o.page == 3 && o.cycle == 0); }
    CHECK(Settings::deserialize(buf, 8, x) && x == s);   // longer (future) records still readable

    fake::reset();
    FlashStore flash = open();
    SettingsStore store(flash);
    Settings l; CHECK(!store.load(l));
    CHECK(store.save(s) && store.load(l) && l == s);
}

struct Saves { int count = 0; Settings last; bool ok = true; };
static bool save_cb(void* ctx, const Settings& s) { auto* c = static_cast<Saves*>(ctx); c->count++; c->last = s; return c->ok; }

static void test_saver() {
    const uint32_t S = SettingsSaver::kSettleMs;      // settle time under test
    Saves saves;
    Settings stored; stored.backlight = 255; stored.page = 0;
    SettingsSaver saver(stored, save_cb, &saves);

    Settings w = stored;
    saver.update(w, 0); saver.update(w, 10 * S);
    CHECK(saves.count == 0);                          // unchanged: never written

    const uint32_t t0 = 20 * S;
    w.backlight = 200;
    saver.update(w, t0);                              // change: timer starts
    saver.update(w, t0 + S - 1);
    CHECK(saves.count == 0);
    w.backlight = 190;                                // still changing (holding the button): timer restarts
    saver.update(w, t0 + S);
    saver.update(w, t0 + 2 * S - 1);
    CHECK(saves.count == 0);
    saver.update(w, t0 + 2 * S);                      // settled for a full settle time
    CHECK(saves.count == 1 && saves.last.backlight == 190);
    saver.update(w, t0 + 10 * S);
    CHECK(saves.count == 1);                          // nothing more to store

    // change and revert before settling: nothing is written
    const uint32_t t1 = t0 + 20 * S;
    w.page = 2; saver.update(w, t1);
    w.page = 0; w.backlight = 190; saver.update(w, t1 + 100);
    saver.update(w, t1 + 10 * S);
    CHECK(saves.count == 1);

    // write failure: retried after another settle period
    const uint32_t t2 = t1 + 20 * S;
    saves.ok = false;
    w.page = 1; saver.update(w, t2);
    saver.update(w, t2 + S);
    CHECK(saves.count == 2);                          // attempt failed
    saver.update(w, t2 + 2 * S - 1);
    CHECK(saves.count == 2);
    saves.ok = true;
    saver.update(w, t2 + 2 * S);
    CHECK(saves.count == 3 && saves.last.page == 1);
    saver.update(w, t2 + 20 * S);
    CHECK(saves.count == 3);

    // millisecond counter wrap does not break the settle arithmetic
    Saves s2; SettingsSaver sv(stored, save_cb, &s2);
    Settings wrapped = stored; wrapped.page = 3;
    sv.update(wrapped, 0xFFFFFF00u);
    sv.update(wrapped, 0xFFFFFF00u + S);
    CHECK(s2.count == 1);
}

int main() {
    test_basic();
    test_rotation_and_wear();
    test_corruption_and_power_loss();
    test_settings();
    test_saver();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all settings/flash-store tests passed\n");
    return 0;
}
