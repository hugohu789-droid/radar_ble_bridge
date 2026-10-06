#pragma once

/*
 * UART protocol shared with the Nucleo radar firmware.
 *
 * Every frame is "$<body>*<CS>" followed by CR LF, where CS is the XOR
 * of every character of <body>, as two hex digits.
 *
 *   Nucleo -> bridge  $R,<angle>,<distance_mm>,<status>   radar sample
 *                     $A,<warn_mm>,<danger_mm>            thresholds in effect
 *   bridge -> Nucleo  $T,<warn_mm>,<danger_mm>            set thresholds
 *                     $Q                                  query thresholds
 *
 * This module has no hardware or SDK dependencies, so it can be unit
 * tested on a PC.
 */

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace radar {

/* Matches enum radar_status in the Nucleo firmware */
enum class Status : uint8_t {
    Ok = 0,
    OutOfRange = 1,
    NoReply = 2,
};

struct Sample {
    uint8_t angle_deg;
    Status status;
    uint16_t distance_mm;
};

struct Thresholds {
    uint16_t warn_mm;
    uint16_t danger_mm;
};

/* Valid threshold range, matching the US-100 range and the Nucleo check */
constexpr uint16_t kThresholdMinMm = 20;
constexpr uint16_t kThresholdMaxMm = 4500;

/* MIN <= danger < warn <= MAX */
bool thresholds_valid(const Thresholds &t);

struct Frame {
    enum class Kind { Sample, Thresholds };

    Kind kind;
    Sample sample;          /* valid when kind == Sample */
    Thresholds thresholds;  /* valid when kind == Thresholds */
};

/* XOR of all characters */
uint8_t checksum(std::string_view body);

/*
 * Parse one line without the trailing CR/LF.
 * Returns nothing if the framing, checksum, type or any field is invalid.
 */
std::optional<Frame> parse_line(std::string_view line);

/*
 * Format commands for the Nucleo, including CR LF. Return the number of
 * characters written (not counting the terminating NUL), or 0 if the
 * buffer is too small.
 */
size_t format_set_thresholds(const Thresholds &t, char *buf, size_t size);
size_t format_query_thresholds(char *buf, size_t size);

} // namespace radar
