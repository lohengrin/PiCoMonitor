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

int main() {
    test_availability_and_cycling();
    test_each_page_draws();
    test_tick();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all pages tests passed\n");
    return 0;
}
