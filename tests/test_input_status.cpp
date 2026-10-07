#include "Input.h"
#include "Status.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void test_long_press() {
    RepeatFilter r; uint8_t pressed, lp;
    r.update(CornerBottomRight, 0, &pressed, &lp);
    CHECK(pressed == CornerBottomRight && lp == 0);
    for (uint32_t t = 10; t < RepeatFilter::kLongPressMs; t += 10) {
        r.update(CornerBottomRight, t, &pressed, &lp);
        CHECK(lp == 0);
    }
    r.update(CornerBottomRight, RepeatFilter::kLongPressMs, &pressed, &lp);
    CHECK(lp == CornerBottomRight);                       // reported once...
    r.update(CornerBottomRight, RepeatFilter::kLongPressMs + 10, &pressed, &lp);
    CHECK(lp == 0);
    r.update(CornerBottomRight, 5000, &pressed, &lp);
    CHECK(lp == 0);
    r.update(0, 5010, &pressed, &lp);                     // ...released, then again on the next long hold
    r.update(CornerTopRight, 6000, &pressed, &lp);
    CHECK(lp == 0);
    r.update(CornerTopRight, 6000 + RepeatFilter::kLongPressMs, &pressed, &lp);
    CHECK(lp == CornerTopRight);
    // a short tap never reports it
    RepeatFilter q;
    q.update(CornerTopRight, 0, &pressed, &lp); q.update(0, 300, &pressed, &lp);
    q.update(CornerTopRight, 400, &pressed, &lp); q.update(0, 700, &pressed, &lp);
    CHECK(lp == 0);
}

static void test_repeat() {
    // press fires once immediately, nothing while no time has passed
    { RepeatFilter r;
      CHECK(r.update(CornerTopLeft, 0) == CornerTopLeft);
      CHECK(r.update(CornerTopLeft, 10) == 0);
      CHECK(r.update(CornerTopLeft, 200) == 0);                       // not yet "> repeat"
      CHECK(r.update(CornerTopLeft, 201) == CornerTopLeft);           // first repeat
      CHECK(r.update(CornerTopLeft, 300) == 0);
      CHECK(r.update(CornerTopLeft, 402) == CornerTopLeft); }
    // after > 1 s held, repeats come 3x faster
    { RepeatFilter r; uint32_t t = 0; int fired = 0;
      r.update(CornerBottomLeft, t);
      for (t = 10; t <= 1000; t += 10) fired += r.update(CornerBottomLeft, t) ? 1 : 0;
      CHECK(fired >= 4 && fired <= 5);                                // ~200 ms rate
      fired = 0;
      for (t = 1010; t <= 2000; t += 10) fired += r.update(CornerBottomLeft, t) ? 1 : 0;
      CHECK(fired >= 13 && fired <= 15); }                            // ~66 ms rate
    // releasing re-arms: next press fires immediately
    { RepeatFilter r;
      CHECK(r.update(CornerTopRight, 0) == CornerTopRight);
      CHECK(r.update(0, 50) == 0);
      CHECK(r.update(CornerTopRight, 60) == CornerTopRight); }
    // corners are independent
    { RepeatFilter r;
      CHECK(r.update(CornerTopLeft | CornerBottomRight, 0) == (CornerTopLeft | CornerBottomRight));
      CHECK(r.update(CornerTopLeft, 100) == 0);
      CHECK(r.update(CornerTopLeft | CornerBottomLeft, 110) == CornerBottomLeft); }
    // monotonic ms wrap (uint32) does not break repeat arithmetic
    { RepeatFilter r; uint32_t t = 0xFFFFFF00u;
      CHECK(r.update(CornerTopLeft, t) == CornerTopLeft);
      CHECK(r.update(CornerTopLeft, t + 0x150) == CornerTopLeft); }
}

static void test_pressed_edges() {
    RepeatFilter r; uint8_t pressed = 0xFF;
    CHECK(r.update(CornerTopRight, 0, &pressed) == CornerTopRight && pressed == CornerTopRight);
    // a held corner repeats as "fired" but is never "pressed" again
    CHECK(r.update(CornerTopRight, 201, &pressed) == CornerTopRight && pressed == 0);
    CHECK(r.update(CornerTopRight, 500, &pressed) == CornerTopRight && pressed == 0);
    CHECK(r.update(0, 600, &pressed) == 0 && pressed == 0);
    CHECK(r.update(CornerTopRight, 610, &pressed) == CornerTopRight && pressed == CornerTopRight);
    // two corners pressed at once are both reported
    RepeatFilter r2;
    r2.update(0, 0, &pressed);
    CHECK(r2.update(CornerBottomLeft | CornerBottomRight, 10, &pressed) == (CornerBottomLeft | CornerBottomRight));
    CHECK(pressed == (CornerBottomLeft | CornerBottomRight));
}

static void test_corners() {
    const int W = 320, H = 240;
    CHECK(corner_at(0, 0, W, H) == CornerTopLeft);
    CHECK(corner_at(105, 79, W, H) == CornerTopLeft);          // last pixel of the zone
    CHECK(corner_at(106, 79, W, H) == 0);                      // outside in x
    CHECK(corner_at(105, 80, W, H) == 0);                      // outside in y
    CHECK(corner_at(0, 239, W, H) == CornerBottomLeft);
    CHECK(corner_at(319, 0, W, H) == CornerTopRight);
    CHECK(corner_at(319, 239, W, H) == CornerBottomRight);
    CHECK(corner_at(160, 120, W, H) == 0);                     // centre
    CHECK(corner_at(160, 0, W, H) == 0);                       // top middle
    CHECK(corner_at(0, 120, W, H) == 0);                       // left middle
}

static MonitorData frame(double cpu, bool has_ram, double ram, bool has_temp, double temp, double disk_pct) {
    MonitorData d;
    d.cpu_percent = {cpu, cpu};
    d.has_ram = has_ram; d.ram = ram; d.has_temp = has_temp; d.temp = temp;
    d.disks.push_back({"C:", 100.0, disk_pct});
    return d;
}

static void test_status() {
    CHECK(computeStatus(frame(10, true, 30, true, 40, 50)) == Status::Ok);
    CHECK(computeStatus(frame(75, true, 30, true, 40, 50)) == Status::Warning);     // CPU
    CHECK(computeStatus(frame(95, true, 30, true, 40, 50)) == Status::Critical);
    CHECK(computeStatus(frame(10, true, 85, true, 40, 50)) == Status::Warning);     // RAM
    CHECK(computeStatus(frame(10, true, 96, true, 40, 50)) == Status::Critical);
    CHECK(computeStatus(frame(10, true, 30, true, 72, 50)) == Status::Warning);     // temperature
    CHECK(computeStatus(frame(10, true, 30, true, 90, 50)) == Status::Critical);
    CHECK(computeStatus(frame(10, true, 30, true, 40, 91)) == Status::Warning);     // disk
    CHECK(computeStatus(frame(10, true, 30, true, 40, 99)) == Status::Critical);
    // worst of several wins
    CHECK(computeStatus(frame(75, true, 96, true, 40, 50)) == Status::Critical);
    // absent values are ignored (a stale 0 default must not matter, nor a huge unset one)
    CHECK(computeStatus(frame(10, false, 99, false, 99, 50)) == Status::Ok);
    // empty frame / zero-size disk
    MonitorData e; CHECK(computeStatus(e) == Status::Ok);
    MonitorData z; z.disks.push_back({"x", 0.0, 50.0}); CHECK(computeStatus(z) == Status::Ok);
}

int main() {
    test_repeat();
    test_long_press();
    test_corners();
    test_pressed_edges();
    test_status();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all input/status tests passed\n");
    return 0;
}
