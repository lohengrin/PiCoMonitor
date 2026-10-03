#include "Protocol.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const char* kSample =
    R"({"CPU": [0.0, 2.0, 2.0, 4.0, 4.0, 0.0, 0.0, 2.0, 3.9, 2.0, 0.0, 2.0], "TEMP": 37.0, "RAM": 32.0, )"
    R"("DISKS": [{"path": "/", "total": 982.83, "used": 517.9}, {"path": "Data", "total": 1968.67, "used": 569.8}, )"
    R"({"path": "DataRO", "total": 1968.67, "used": 569.8}, {"path": "Video", "total": 3937.8, "used": 3322.62}]})";

// Feed a string; returns the frames completed (as std::string copies)
struct Rx {
    char buf[1024];
    FrameAssembler fa{buf, sizeof(buf)};
    int frames = 0;
    std::string last;
    void feed(const std::string& s, size_t chunk = 0) {
        (void)chunk;
        for (char c : s)
            if (fa.feed(c)) { frames++; last.assign(fa.buffer(), fa.length()); }
    }
};

static MonitorData decode(const std::string& s, bool* ok = nullptr) {
    MonitorData d;
    bool r = decode_data(s.data(), s.size(), d);
    if (ok) *ok = r;
    return d;
}

static void test_assembler() {
    { Rx rx; rx.feed(kSample); CHECK(rx.frames == 1); CHECK(rx.last == kSample); }
    // garbage and stray closing braces between frames are ignored
    { Rx rx; rx.feed(std::string("xx}}\n\r") + kSample + "}\n" + kSample);
      CHECK(rx.frames == 2); CHECK(rx.last == kSample); }
    // braces and escaped quotes inside strings do not confuse the depth
    { Rx rx; std::string f = R"({"DISKS":[{"path":"a}\"{b","total":1,"used":0}]})";
      rx.feed(f); CHECK(rx.frames == 1); CHECK(rx.last == f); }
    // frame after a complete one starts fresh (buffer not appended to)
    { Rx rx; rx.feed(R"({"RAM":1})"); rx.feed(R"({"RAM":2})"); CHECK(rx.frames == 2); CHECK(rx.last == R"({"RAM":2})"); }
    // overflow: counted, then resynchronises on the next frame
    { char small[32]; FrameAssembler fa(small, sizeof small); int frames = 0;
      std::string big = std::string(R"({"a":")") + std::string(100, 'x') + "\"}";
      for (char c : big) frames += fa.feed(c);
      CHECK(frames == 0); CHECK(fa.overflows() >= 1); CHECK(!fa.in_frame() || fa.length() < sizeof small);
      for (char c : std::string(R"({"RAM":3})")) frames += fa.feed(c);
      CHECK(frames == 1); CHECK(std::string(fa.buffer(), fa.length()) == R"({"RAM":3})"); }
    // reset() drops a partial frame; the next frame is clean
    { Rx rx; rx.feed(R"({"CPU":[1,2)"); CHECK(rx.fa.in_frame()); rx.fa.reset(); CHECK(!rx.fa.in_frame());
      rx.feed(R"({"RAM":4})"); CHECK(rx.frames == 1); CHECK(rx.last == R"({"RAM":4})"); }
    // random garbage never crashes and never reports a frame without a '{'
    { Rx rx; srand(1); for (int i = 0; i < 200000; i++) { char c = (char)(rand() & 0xFF); if (c == '{') c = '['; rx.fa.feed(c); }
      CHECK(rx.frames == 0); }
}

static const char* kFullSample =
    R"({"CPU": [0.0, 2.0], "TEMP": 48.9, "RAM": 34.0, "DISKS": [{"path": "/", "total": 982.83, "used": 517.9}], )"
    R"("NET": [1.6, 3.6], "IO": [3091.6, 464.5], "FREQ": 3078.0, "LOAD": [1.77, 1.38, 1.14], "SWAP": 98.5, )"
    R"("UP": 335821, "GPU": {"n": "Quadro P4000", "l": 0.0, "t": 38.0, "mu": 6717, "mt": 8192}})";

static void test_extras() {
    bool ok;
    MonitorData d = decode(kFullSample, &ok);
    CHECK(ok);
    CHECK(d.has_net && d.net_down == 1.6 && d.net_up == 3.6);
    CHECK(d.has_io && d.io_read == 3091.6 && d.io_write == 464.5);
    CHECK(d.has_freq && d.freq_mhz == 3078.0);
    CHECK(d.has_load && d.load_avg[0] == 1.77 && d.load_avg[2] == 1.14);
    CHECK(d.has_swap && d.swap == 98.5);
    CHECK(d.has_uptime && d.uptime_s == 335821);
    CHECK(d.gpus.size() == 1 && d.gpus[0].name == "Quadro P4000" && d.gpus[0].has_load && d.gpus[0].has_temp);
    CHECK(d.gpus[0].has_vram && d.gpus[0].vram_used_mb == 6717 && d.gpus[0].vram_total_mb == 8192);

    // an old host sends none of them: everything stays absent
    d = decode(kSample, &ok);
    CHECK(ok && !d.has_net && !d.has_io && !d.has_freq && !d.has_load && !d.has_swap && !d.has_uptime && d.gpus.empty());

    // odd shapes are ignored, never fatal
    d = decode(R"({"NET":[1],"IO":"x","FREQ":null,"LOAD":[1,2],"SWAP":[1],"UP":-5,"GPU":5})", &ok);
    CHECK(ok && !d.has_net && !d.has_io && !d.has_freq && !d.has_load && !d.has_swap && !d.has_uptime && d.gpus.empty());
    d = decode(R"({"NET":[1,"a"],"LOAD":[1,2,"x"],"UP":1e30})", &ok);
    CHECK(ok && !d.has_net && !d.has_load && !d.has_uptime);
    // GPU with partial / missing fields (e.g. AMD without temperature)
    d = decode(R"({"GPU":{"n":"AMD GPU","l":42}})", &ok);
    CHECK(ok && d.gpus.size() == 1 && d.gpus[0].has_load && !d.gpus[0].has_temp && !d.gpus[0].has_vram);
    d = decode(R"({"GPU":{}})", &ok);
    CHECK(ok && d.gpus.size() == 1 && d.gpus[0].name.empty() && !d.gpus[0].has_load);
    // GPU name is capped
    d = decode(R"({"GPU":{"n":"0123456789012345678901234567890123456789"}})", &ok);
    CHECK(ok && d.gpus[0].name.size() == MonitorData::kMaxGpuName);
    // several GPUs (discrete + integrated): an array, order kept, capped at kMaxGpus
    d = decode(R"({"GPU":[{"n":"Quadro P4000","l":0,"t":38,"mu":6717,"mt":8192},{"n":"Radeon Vega Series","l":6,"t":37,"mu":1019,"mt":4096}]})", &ok);
    CHECK(ok && d.gpus.size() == 2);
    CHECK(d.gpus[0].name == "Quadro P4000" && d.gpus[1].name == "Radeon Vega Series");
    CHECK(d.gpus[1].has_load && d.gpus[1].load == 6 && d.gpus[1].vram_total_mb == 4096);
    d = decode(R"({"GPU":[{"n":"A"},{"n":"B"},{"n":"C"},{"n":"D"}]})", &ok);
    CHECK(ok && d.gpus.size() == MonitorData::kMaxGpus && d.gpus[1].name == "B");
    // junk entries in the array are skipped
    d = decode(R"({"GPU":[5,null,{"n":"Real"},"x"]})", &ok);
    CHECK(ok && d.gpus.size() == 1 && d.gpus[0].name == "Real");
    d = decode(R"({"GPU":[]})", &ok);
    CHECK(ok && d.gpus.empty());
    // a full frame fits the firmware's frame buffer comfortably
    CHECK(strlen(kFullSample) < 1024);
}

static void test_decode() {
    bool ok;
    MonitorData d = decode(kSample, &ok);
    CHECK(ok); CHECK(d.cpu_percent.size() == 12); CHECK(d.has_temp && d.temp == 37.0);
    CHECK(d.has_ram && d.ram == 32.0); CHECK(d.disks.size() == 4);
    CHECK(d.disks[0].label == "/" && std::fabs(d.disks[0].used - 517.9) < 1e-9);
    CHECK(d.disks[3].label == "Video" && std::fabs(d.disks[3].total - 3937.8) < 1e-9);

    // null / missing / wrongly-typed values: no crash, flags stay false
    d = decode(R"({"CPU":[1,null,"x",3],"TEMP":null,"RAM":"50"})", &ok);
    CHECK(ok); CHECK(d.cpu_percent.size() == 2); CHECK(!d.has_temp); CHECK(!d.has_ram);
    d = decode(R"({})", &ok);  CHECK(ok); CHECK(!d.has_temp && !d.has_ram && d.cpu_percent.empty() && d.disks.empty());
    d = decode(R"({"TEMP":true,"RAM":[1],"CPU":5,"DISKS":"no"})", &ok); CHECK(ok); CHECK(!d.has_temp && !d.has_ram);
    // disk entries of the wrong type / with bad fields are tolerated, defaults are zero (not garbage)
    d = decode(R"({"DISKS":[1,"a",null,{"path":5,"total":"x"},{"path":"E"}]})", &ok);
    CHECK(ok); CHECK(d.disks.size() == 2); CHECK(d.disks[0].total == 0.0 && d.disks[0].used == 0.0);
    CHECK(d.disks[1].label == "E");
    // not an object / not JSON
    decode("[1,2]", &ok); CHECK(!ok);
    decode("{\"CPU\": [1,", &ok); CHECK(!ok);
    decode("", &ok); CHECK(!ok);
    decode("{broken}", &ok); CHECK(!ok);
    // limits
    std::string many = "{\"CPU\":[";
    for (int i = 0; i < 200; i++) many += (i ? ",1" : "1");
    many += "],\"DISKS\":[";
    for (int i = 0; i < 20; i++) many += (i ? ",{\"path\":\"d\"}" : "{\"path\":\"d\"}");
    many += "]}";
    d = decode(many, &ok); CHECK(ok); CHECK(d.cpu_percent.size() == MonitorData::kMaxCores);
    CHECK(d.disks.size() == MonitorData::kMaxDisks);
}

int main() {
    test_assembler();
    test_decode();
    test_extras();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all protocol tests passed\n");
    return 0;
}
