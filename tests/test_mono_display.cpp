#include "MonoDisplay.h"

#include <cstdio>
#include <vector>

using namespace pico_toolset;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// bit of the native buffer (portrait 122x250): true = white
static bool white(const std::vector<uint8_t>& fb, int nx, int ny) { return fb[ny * 16 + nx / 8] & (0x80 >> (nx & 7)); }

int main() {
    std::vector<uint8_t> fb(16 * 250);
    MonoDisplay d(fb.data(), 122, 250);
    CHECK(d.width() == 250 && d.height() == 122);

    // starts white
    for (uint8_t b : fb) CHECK(b == 0xFF);

    // landscape (0,0) = top-left maps to native (121, 0); (249,121) to native (0, 249)
    d.set_pixel(0, 0, kColorWhite);
    CHECK(!white(fb, 121, 0));
    d.set_pixel(249, 121, kColorGreen);
    CHECK(!white(fb, 0, 249));
    d.set_pixel(10, 20, kColorWhite);
    CHECK(!white(fb, 121 - 20, 10));

    // only that one pixel changed per set
    int ink = 0;
    for (int y = 0; y < 250; ++y) for (int x = 0; x < 122; ++x) ink += white(fb, x, y) ? 0 : 1;
    CHECK(ink == 3);

    // black is paper again; dim colors below the threshold are paper, blue and the border are ink
    d.set_pixel(0, 0, kColorBlack);
    CHECK(white(fb, 121, 0));
    CHECK(MonoDisplay::is_ink(Color::from_rgb888(10, 10, 255)));
    CHECK(MonoDisplay::is_ink(Color::from_rgb888(0, 50, 100)));
    CHECK(!MonoDisplay::is_ink(Color::from_rgb888(20, 20, 20)));

    // out of range is ignored, fill_rect (default, via set_pixel) clips
    d.set_pixel(-1, 0, kColorWhite); d.set_pixel(250, 0, kColorWhite); d.set_pixel(0, 122, kColorWhite);
    d.fill_rect(240, 110, 300, 200, kColorWhite);
    ink = 0;
    for (int y = 0; y < 250; ++y) for (int x = 0; x < 122; ++x) ink += white(fb, x, y) ? 0 : 1;
    CHECK(ink == 2 + 10 * 12 - 1);

    if (failures == 0) printf("test_mono_display OK\n");
    return failures ? 1 : 0;
}
