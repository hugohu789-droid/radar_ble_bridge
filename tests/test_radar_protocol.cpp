/*
 * Host-side unit tests for radar_protocol (no Pico SDK needed).
 *
 * Build and run on a PC, e.g. with the MinGW g++ that ships with Qt:
 *   g++ -std=c++20 -Wall -Wextra -I../src ../src/radar_protocol.cpp test_radar_protocol.cpp -o test_radar_protocol
 *   ./test_radar_protocol
 */

#include "radar_protocol.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

/* Build a correctly framed line for a body, without CR LF */
std::string frame(const std::string &body)
{
    char cs[3];
    std::snprintf(cs, sizeof(cs), "%02X", radar::checksum(body));
    return "$" + body + "*" + cs;
}

void test_sample()
{
    auto f = radar::parse_line("$R,90,1234,0*43");
    check(f.has_value(), "known-good sample parses");
    check(f && f->kind == radar::Frame::Kind::Sample, "sample kind");
    check(f && f->sample.angle_deg == 90, "sample angle");
    check(f && f->sample.distance_mm == 1234, "sample distance");
    check(f && f->sample.status == radar::Status::Ok, "sample status");

    check(radar::parse_line(frame("R,0,0,1")).has_value(), "angle 0, out of range");
    check(radar::parse_line(frame("R,180,65535,2")).has_value(), "upper bounds");
}

void test_thresholds_frame()
{
    auto f = radar::parse_line(frame("A,400,150"));
    check(f && f->kind == radar::Frame::Kind::Thresholds, "thresholds kind");
    check(f && f->thresholds.warn_mm == 400 && f->thresholds.danger_mm == 150,
          "thresholds values");
}

void test_rejects()
{
    check(!radar::parse_line("$R,90,1234,0*44"), "wrong checksum");
    check(!radar::parse_line("R,90,1234,0*43"), "missing $");
    check(!radar::parse_line("$R,90,1234,0"), "missing checksum");
    check(!radar::parse_line("$R,90,1234,0*4"), "short checksum");
    check(!radar::parse_line("$R,90,1234,0*ZZ"), "non-hex checksum");
    check(!radar::parse_line(frame("R,181,100,0")), "angle above 180");
    check(!radar::parse_line(frame("R,90,70000,0")), "distance above 16 bits");
    check(!radar::parse_line(frame("R,90,100,3")), "unknown status");
    check(!radar::parse_line(frame("R,90,,0")), "empty field");
    check(!radar::parse_line(frame("R,90,12a,0")), "trailing garbage in field");
    check(!radar::parse_line(frame("R,90,100")), "too few fields");
    check(!radar::parse_line(frame("R,90,100,0,1")), "too many fields");
    check(!radar::parse_line(frame("X,1,2,3")), "unknown type");
    check(!radar::parse_line(""), "empty line");
}

void test_threshold_validation()
{
    check(radar::thresholds_valid({400, 150}), "400/150 valid");
    check(radar::thresholds_valid({4500, 20}), "range limits valid");
    check(!radar::thresholds_valid({150, 400}), "danger above warn");
    check(!radar::thresholds_valid({400, 400}), "danger equal to warn");
    check(!radar::thresholds_valid({400, 19}), "danger below minimum");
    check(!radar::thresholds_valid({4501, 150}), "warn above maximum");
}

void test_format()
{
    char buf[32];
    size_t n = radar::format_set_thresholds({400, 150}, buf, sizeof(buf));
    std::string line(buf, n);
    check(n > 0 && line.size() >= 2 && line.substr(line.size() - 2) == "\r\n",
          "set command ends with CR LF");
    check(line == frame("T,400,150") + "\r\n", "set command framing");

    n = radar::format_query_thresholds(buf, sizeof(buf));
    check(std::string(buf, n) == frame("Q") + "\r\n", "query command framing");

    char tiny[4];
    check(radar::format_set_thresholds({400, 150}, tiny, sizeof(tiny)) == 0,
          "too-small buffer returns 0");
}

} // namespace

int main()
{
    test_sample();
    test_thresholds_frame();
    test_rejects();
    test_threshold_validation();
    test_format();

    if (failures == 0) {
        std::printf("All radar_protocol tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
