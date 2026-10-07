#include "Pages.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace pico_toolset;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

struct Fake : DisplayDriver {
    int w, h;
    std::vector<uint16_t> fb;
    Fake(int w, int h) : w(w), h(h), fb(static_cast<size_t>(w) * h, 0) {}
    int width() const override { return w; }
    int height() const override { return h; }
    void set_pixel(int x, int y, Color c) override { if (x >= 0 && y >= 0 && x < w && y < h) fb[static_cast<size_t>(y) * w + x] = c.rgb565; }
};

static MonitorData decode(const char* json) {
    MonitorData d;
    CHECK(decode_data(json, strlen(json), d));
    return d;
}

static const char* kBasic = R"({"CPU":[1,2],"TEMP":40,"RAM":30,"DISKS":[{"path":"C:","total":100,"used":50}]})";
static const char* kNet = R"({"CPU":[1,2],"NET":[1.5,2.5]})";
static const char* kSys = R"({"FREQ":3000,"UP":90061})";
static const char* kGpu = R"({"GPU":{"n":"X","l":10,"t":50,"mu":1024,"mt":2048}})";

static const char* kGpu2 = R"({"GPU":[{"n":"Quadro P4000","l":10,"t":38,"mu":6717,"mt":8192},{"n":"Radeon Vega Series","l":6,"t":37,"mu":1019,"mt":4096}]})";

static bool drew(const Fake& d) { for (uint16_t p : d.fb) if (p) return true; return false; }

static void test_availability_and_cycling() {
    Fake d(240, 135); Screen s(d); Pages p(s);

    // only the Overview exists until the host sends the matching data
    CHECK(p.current() == Pages::Overview && p.available(Pages::Overview));
    CHECK(!p.available(Pages::Network) && !p.available(Pages::System) && !p.available(Pages::Gpu));
    p.update(decode(kBasic));
    CHECK(!p.available(Pages::Network));
    p.next(); CHECK(p.current() == Pages::Overview);          // nothing to switch to
    p.prev(); CHECK(p.current() == Pages::Overview);
    CHECK(p.toast() == "1/1 Overview");

    p.update(decode(kNet));
    CHECK(p.available(Pages::Network) && !p.available(Pages::System));
    p.next(); CHECK(p.current() == Pages::Network);
    CHECK(p.toast() == "2/2 Network");
    p.next(); CHECK(p.current() == Pages::Overview);          // wraps
    p.prev(); CHECK(p.current() == Pages::Network);            // wraps backwards

    p.update(decode(kGpu));
    p.update(decode(kSys));
    CHECK(p.available(Pages::System) && p.available(Pages::Gpu));
    p.prev(); CHECK(p.current() == Pages::Overview);
    p.prev(); CHECK(p.current() == Pages::Gpu);
    CHECK(p.toast() == "4/4 GPU");
    p.prev(); CHECK(p.current() == Pages::System);
    CHECK(p.toast() == "3/4 System");

    // pages stay available once seen, even if a later frame lacks the data
    p.update(decode(kBasic));
    CHECK(p.available(Pages::Network) && p.available(Pages::Gpu));
}

static void test_preferred_page() {
    Fake d(240, 135); Screen s(d); Pages p(s);
    p.update(decode(kBasic));
    // restore a page that does not exist yet: stays on Overview but remembers the wish
    p.set_preferred(Pages::Gpu);
    CHECK(p.current() == Pages::Overview && p.preferred() == Pages::Gpu);   // persisted value keeps the wish
    p.update(decode(kBasic));
    CHECK(p.current() == Pages::Overview);
    p.update(decode(kGpu));                          // data arrives: switch
    CHECK(p.current() == Pages::Gpu && p.preferred() == Pages::Gpu);

    // a user switch cancels a pending request
    Fake d2(240, 135); Screen s2(d2); Pages q(s2);
    q.update(decode(kBasic));
    q.set_preferred(Pages::System);
    q.update(decode(kNet));                          // Network becomes available
    q.next();                                        // user goes to Network
    CHECK(q.current() == Pages::Network && q.preferred() == Pages::Network);
    q.update(decode(kSys));                          // System appears: must NOT jump there
    CHECK(q.current() == Pages::Network);

    // already available: immediate; out-of-range ignored
    q.set_preferred(Pages::System);
    CHECK(q.current() == Pages::System);
    q.set_preferred(static_cast<Pages::Id>(99));
    CHECK(q.current() == Pages::System);
}

static size_t lit_pixels(const Fake& d, int x0, int y0, int x1, int y1) {
    size_t n = 0;
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) if (d.fb[static_cast<size_t>(y) * d.w + x]) ++n;
    return n;
}

static void test_two_gpus() {
    Fake d(240, 135); Screen s(d); Pages p(s);
    p.update(decode(kBasic));
    CHECK(!p.available(Pages::Gpu));
    p.update(decode(kGpu));                          // one GPU: the page exists, single layout
    CHECK(p.available(Pages::Gpu));
    p.next(); CHECK(p.current() == Pages::Gpu);

    // a second GPU shows up while the page is displayed: both are drawn in columns
    for (int i = 0; i < 20; ++i) p.update(decode(kGpu2));
    std::fill(d.fb.begin(), d.fb.end(), 0);
    s.update();
    const int hw = d.w / 2, hh = d.h / 2;
    CHECK(lit_pixels(d, 0, 0, hw, hh) > 100);        // GPU1 load graph (top-left)
    CHECK(lit_pixels(d, hw, 0, d.w, hh) > 100);      // GPU2 load graph (top-right)
    CHECK(lit_pixels(d, 0, hh, hw, d.h) > 50);       // GPU1 info (bottom-left)
    CHECK(lit_pixels(d, hw, hh, d.w, d.h) > 50);     // GPU2 info (bottom-right)

    // a later frame with one GPU (e.g. a read failed) keeps the two-GPU layout
    p.update(decode(kGpu));
    std::fill(d.fb.begin(), d.fb.end(), 0);
    s.update();
    CHECK(lit_pixels(d, hw, hh, d.w, d.h) > 50);

    // starting directly with two GPUs
    Fake d2(320, 240); Screen s2(d2); Pages q(s2);
    q.update(decode(kBasic)); q.update(decode(kGpu2));
    q.next(); CHECK(q.current() == Pages::Gpu);
    s2.update(); CHECK(drew(d2));
}

static void test_each_page_draws() {
    Fake d(320, 240); Screen s(d); Pages p(s);
    p.update(decode(kBasic)); p.update(decode(kNet)); p.update(decode(kSys)); p.update(decode(kGpu));
    for (int i = 0; i < Pages::kCount; ++i) {
        std::fill(d.fb.begin(), d.fb.end(), 0);
        s.update();
        CHECK(drew(d));
        p.next();
    }
}

static void test_tick() {
    Fake d(240, 135); Screen s(d); Pages p(s);
    p.update(decode(R"({"CPU":[90,90]})"));
    CHECK(!p.tick() || true);                       // marker may still be rising on the first tick
    p.update(decode(R"({"CPU":[0,0]})"));           // load drops: the markers fall
    CHECK(p.tick());                                // marker moved -> redraw
    for (int i = 0; i < 200; ++i) p.tick();
    CHECK(!p.tick());                               // settled: nothing to redraw
    // animation of a page that is not visible does not request redraws
    p.update(decode(kNet)); p.next();
    p.update(decode(R"({"CPU":[100,100]})")); p.update(decode(R"({"CPU":[0,0]})"));
    CHECK(!p.tick());
}

static void test_sticky_slow_values() {
    // the host sends disks / system values only when refreshed: frames without them must not blank them
    Fake d(240, 135); Screen s(d); Pages p(s);
    p.update(decode(R"({"CPU":[1,2],"DISKS":[{"path":"C:","total":100,"used":50}],"FREQ":3000,"LOAD":[1,2,3],"SWAP":5,"UP":90061})"));
    p.next();                                              // System
    CHECK(p.current() == Pages::System);
    s.update(); const size_t full = lit_pixels(d, 0, 0, 240, 135);
    p.update(decode(R"({"CPU":[3,4],"TEMP":40})"));        // a fast frame: nothing slow in it
    s.update(); CHECK(lit_pixels(d, 0, 0, 240, 135) == full);
    p.update(decode(R"({"CPU":[3,4],"UP":90100})"));       // one value refreshed: the others stay
    s.update(); CHECK(lit_pixels(d, 0, 0, 240, 135) >= full - 40);
    p.prev();                                              // Overview: disks still drawn
    s.update(); const size_t with_disk = lit_pixels(d, 120, 67, 240, 135);
    p.update(decode(R"({"CPU":[3,4]})")); s.update();
    CHECK(lit_pixels(d, 120, 67, 240, 135) == with_disk);
    p.update(decode(R"({"CPU":[3,4],"DISKS":[]})")); s.update();   // an explicit empty list does clear
    CHECK(lit_pixels(d, 120, 67, 240, 135) != with_disk);
}

static void test_cycling_mode() {
    Fake d(240, 135); Screen s(d); Pages p(s);
    p.update(decode(kBasic)); p.update(decode(kNet)); p.update(decode(kSys));   // Overview, Network, System
    p.next();                                        // manual: Network
    CHECK(p.preferred() == Pages::Network && p.cycle_s() == 0);

    // starts from the current page; the persisted page stays the one it started from
    p.set_cycle(30);
    CHECK(p.cycle_s() == 30 && p.current() == Pages::Network && p.preferred() == Pages::Network);
    CHECK(!p.advance(1000));                         // first call arms the timer
    CHECK(!p.advance(30999));                        // not yet
    CHECK(p.advance(31000) && p.current() == Pages::System);
    CHECK(p.preferred() == Pages::Network && p.cycle_s() == 30);     // nothing new to store
    CHECK(p.advance(61000) && p.current() == Pages::Overview);       // wraps (Gpu not available)
    CHECK(!p.advance(61001));

    // a page that appears joins the cycle; the clock wrap (uint32) is harmless
    p.update(decode(kGpu));
    CHECK(p.advance(91000) && p.current() == Pages::Network);

    // any manual change ends it, and becomes the persisted page
    p.next();
    CHECK(p.cycle_s() == 0 && p.current() == Pages::System && p.preferred() == Pages::System);
    CHECK(!p.advance(200000));

    // a host page command is a manual change too
    p.set_cycle(10);
    p.set_preferred(Pages::Gpu);
    CHECK(p.cycle_s() == 0 && p.current() == Pages::Gpu && p.preferred() == Pages::Gpu);

    // start_cycle() reuses the last period; the toast names it
    p.set_cycle(15); p.set_cycle(0);
    CHECK(p.cycle_s() == 0);
    p.start_cycle();
    CHECK(p.cycle_s() == 15 && p.cycle_toast() == "Cycling 15 s");
    p.set_cycle(0);

    // clock wrap
    p.set_cycle(5);
    CHECK(!p.advance(0xFFFFFF00u));
    CHECK(!p.advance(0xFFFFFF00u + 4000));
    CHECK(p.advance(0xFFFFFF00u + 5000));
    CHECK(p.advance(0xFFFFFF00u + 10000));           // across the 2^32 boundary
}

static void test_restore() {
    // boot with a stored cycling mode whose start page is not available yet: nothing cycles until it is
    Fake d(240, 135); Screen s(d); Pages p(s);
    p.update(decode(kBasic)); p.update(decode(kNet));
    p.restore(Pages::Gpu, 20);
    CHECK(p.current() == Pages::Overview && p.cycle_s() == 20 && p.preferred() == Pages::Gpu);
    CHECK(!p.advance(0)); CHECK(!p.advance(60000));  // inert while the page is pending
    p.update(decode(kGpu));                          // data arrives: shows it, the cycling mode goes on
    CHECK(p.current() == Pages::Gpu && p.cycle_s() == 20 && p.preferred() == Pages::Gpu);
    CHECK(!p.advance(100000));                       // arms
    CHECK(p.advance(120000) && p.current() == Pages::Overview);
    CHECK(p.preferred() == Pages::Gpu);

    // plain restore without cycling; bad page falls back to Overview
    Fake d2(240, 135); Screen s2(d2); Pages q(s2);
    q.restore(static_cast<Pages::Id>(77), 0);
    CHECK(q.current() == Pages::Overview && q.cycle_s() == 0);
}

int main() {
    test_sticky_slow_values();
    test_cycling_mode();
    test_restore();
    test_availability_and_cycling();
    test_preferred_page();
    test_two_gpus();
    test_each_page_draws();
    test_tick();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all pages tests passed\n");
    return 0;
}
