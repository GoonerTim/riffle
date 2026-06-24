#include "riffle/timestamp.hpp"

#include <cctype>
#include <cstdio>
#include <string>

namespace riffle {
namespace {

struct Cal {
    int year, month, day, hour, min, sec;
};

std::int64_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097LL + static_cast<int>(doe) - 719468;
}

bool in_range(const Cal& c) {
    return c.month >= 1 && c.month <= 12 && c.day >= 1 && c.day <= 31 && c.hour >= 0 &&
           c.hour <= 23 && c.min >= 0 && c.min <= 59 && c.sec >= 0 && c.sec <= 60;
}

std::int64_t to_micros(const Cal& c) {
    const std::int64_t days =
        days_from_civil(c.year, static_cast<unsigned>(c.month), static_cast<unsigned>(c.day));
    const std::int64_t secs = days * 86400 + c.hour * 3600LL + c.min * 60LL + c.sec;
    return secs * 1000000;
}

std::int64_t fractional_us(std::string_view text) {
    const auto dot = text.find('.');
    if (dot == std::string_view::npos) return 0;
    std::int64_t scale = 100000, micros = 0;
    for (std::size_t i = dot + 1; i < text.size() && std::isdigit((unsigned char)text[i]); ++i) {
        micros += (text[i] - '0') * scale;
        scale /= 10;
    }
    return micros;
}

}

std::optional<std::int64_t> parse_timestamp_us(std::string_view text) {
    Cal c{};
    const std::string buf(text);
    if (std::sscanf(buf.c_str(), "%d-%d-%dT%d:%d:%d", &c.year, &c.month, &c.day, &c.hour, &c.min,
                    &c.sec) != 6) {
        return std::nullopt;
    }
    if (!in_range(c)) return std::nullopt;
    return to_micros(c) + fractional_us(text);
}

bool looks_like_timestamp(std::string_view text) {
    return parse_timestamp_us(text).has_value();
}

}
