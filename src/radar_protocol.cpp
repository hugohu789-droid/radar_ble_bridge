#include "radar_protocol.h"

#include <array>
#include <charconv>
#include <cstdio>

namespace radar {

namespace {

constexpr int kMaxAngle = 180;
constexpr size_t kMaxFields = 4;

/* Parse a whole decimal field into [0, max]; rejects empty or partial numbers */
std::optional<uint32_t> parse_uint(std::string_view s, uint32_t max)
{
    uint32_t value = 0;
    const char *begin = s.data();
    const char *end = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);

    if (s.empty() || ec != std::errc() || ptr != end || value > max) {
        return std::nullopt;
    }
    return value;
}

std::optional<uint8_t> parse_hex_byte(std::string_view s)
{
    uint32_t value = 0;
    const char *end = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(s.data(), end, value, 16);

    if (s.size() != 2 || ec != std::errc() || ptr != end) {
        return std::nullopt;
    }
    return static_cast<uint8_t>(value);
}

/* Split "R,90,1234,0" on commas. Returns the number of fields, or 0 if too many. */
size_t split_fields(std::string_view body, std::array<std::string_view, kMaxFields> &fields)
{
    size_t count = 0;

    while (true) {
        if (count == kMaxFields) {
            return 0;
        }
        const size_t comma = body.find(',');
        fields[count++] = body.substr(0, comma);
        if (comma == std::string_view::npos) {
            return count;
        }
        body.remove_prefix(comma + 1);
    }
}

/* Write "$<body>*<CS>\r\n" */
size_t format_frame(std::string_view body, char *buf, size_t size)
{
    const int n = std::snprintf(buf, size, "$%.*s*%02X\r\n", static_cast<int>(body.size()),
                                body.data(), checksum(body));

    if (n < 0 || static_cast<size_t>(n) >= size) {
        return 0;
    }
    return static_cast<size_t>(n);
}

} // namespace

bool thresholds_valid(const Thresholds &t)
{
    return t.danger_mm >= kThresholdMinMm && t.warn_mm <= kThresholdMaxMm &&
           t.danger_mm < t.warn_mm;
}

uint8_t checksum(std::string_view body)
{
    uint8_t cs = 0;
    for (char c : body) {
        cs ^= static_cast<uint8_t>(c);
    }
    return cs;
}

std::optional<Frame> parse_line(std::string_view line)
{
    /* Shortest valid frame is "$Q*51" */
    if (line.size() < 5 || line.front() != '$' || line[line.size() - 3] != '*') {
        return std::nullopt;
    }

    const std::string_view body = line.substr(1, line.size() - 4);
    const auto expected = parse_hex_byte(line.substr(line.size() - 2));

    if (!expected || *expected != checksum(body)) {
        return std::nullopt;
    }

    std::array<std::string_view, kMaxFields> f{};
    const size_t count = split_fields(body, f);

    if (count == 4 && f[0] == "R") {
        const auto angle = parse_uint(f[1], kMaxAngle);
        const auto distance = parse_uint(f[2], UINT16_MAX);
        const auto status = parse_uint(f[3], static_cast<uint32_t>(Status::NoReply));

        if (!angle || !distance || !status) {
            return std::nullopt;
        }

        Frame frame{};
        frame.kind = Frame::Kind::Sample;
        frame.sample = {static_cast<uint8_t>(*angle), static_cast<Status>(*status),
                        static_cast<uint16_t>(*distance)};
        return frame;
    }

    if (count == 3 && f[0] == "A") {
        const auto warn = parse_uint(f[1], UINT16_MAX);
        const auto danger = parse_uint(f[2], UINT16_MAX);

        if (!warn || !danger) {
            return std::nullopt;
        }

        Frame frame{};
        frame.kind = Frame::Kind::Thresholds;
        frame.thresholds = {static_cast<uint16_t>(*warn), static_cast<uint16_t>(*danger)};
        return frame;
    }

    return std::nullopt;
}

size_t format_set_thresholds(const Thresholds &t, char *buf, size_t size)
{
    char body[24];
    const int n = std::snprintf(body, sizeof(body), "T,%u,%u", t.warn_mm, t.danger_mm);

    if (n < 0 || static_cast<size_t>(n) >= sizeof(body)) {
        return 0;
    }
    return format_frame(std::string_view(body, static_cast<size_t>(n)), buf, size);
}

size_t format_query_thresholds(char *buf, size_t size)
{
    return format_frame("Q", buf, size);
}

} // namespace radar
