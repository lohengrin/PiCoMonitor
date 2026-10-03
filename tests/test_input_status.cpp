#include "Input.h"
#include "Status.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

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
    test_status();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all input/status tests passed\n");
    return 0;
}
