#include "Conversion.h"

#include <algorithm>
#include <ios>
#include <string>
#include <stdexcept>
#include <limits>
#include <sstream>

IntConversionResult safeStrToInt(const std::string &str) {
    IntConversionResult result;
    try {
        size_t pos = 0;
        const int value = std::stoi(str, &pos);
        if (pos == str.size()) {
            result.value = value;
            result.success = true;
        }
    } catch (const std::exception &) {
        result.success = false;
    }
    return result;
}

LongConversionResult safeStrToLong(const std::string &str) {
    LongConversionResult result;
    try {
        size_t pos = 0;
        const long value = std::stol(str, &pos);
        if (pos == str.size()) {
            result.value = value;
            result.success = true;
        }
    } catch (const std::exception &) {
        result.success = false;
    }
    return result;
}

FloatConversionResult safeStrToFloat(const std::string &str) {
    FloatConversionResult result;
    try {
        size_t pos = 0;
        const float value = std::stof(str, &pos);
        if (pos == str.size()) {
            result.value = value;
            result.success = true;
        }
    } catch (const std::exception &) {
        result.success = false;
    }
    return result;
}

UInt16ConversionResult safeStrToUInt16(const std::string &str) {
    UInt16ConversionResult result;
    try {
        size_t pos = 0;
        if (const unsigned long value = std::stoul(str, &pos, 0);
            pos == str.size() && value <= std::numeric_limits<uint16_t>::max()) {
            result.value = static_cast<uint16_t>(value);
            result.success = true;
        }
    } catch (const std::exception &) {
        result.success = false;
    }
    return result;
}

UInt32ConversionResult safeStrToUInt32(const std::string &str) {
    UInt32ConversionResult result;
    try {
        size_t pos = 0;
        if (const unsigned long value = std::stoul(str, &pos, 0);
            pos == str.size() && value <= std::numeric_limits<uint32_t>::max()) {
            result.value = static_cast<uint32_t>(value);
            result.success = true;
        }
    } catch (const std::exception &) {
        result.success = false;
    }
    return result;
}

StringConversionResult safeIntToStr(const int value) {
    StringConversionResult result;
    result.str = std::to_string(value);
    result.success = true;
    return result;
}

StringConversionResult safeLongToStr(const long value) {
    StringConversionResult result;
    result.str = std::to_string(value);
    result.success = true;
    return result;
}

StringConversionResult safeUInt16ToStr(const uint16_t value) {
    StringConversionResult result;
    result.str = std::to_string(value);
    result.success = true;
    return result;
}

StringConversionResult safeUInt32ToStr(const uint32_t value) {
    StringConversionResult result;
    result.str = std::to_string(value);
    result.success = true;
    return result;
}

StringConversionResult safeFloatToStr(const float value) {
    StringConversionResult result;
    try {
        std::ostringstream oss;
        oss << std::fixed << value;
        result.str = oss.str();
        result.success = true;
    } catch (const std::exception &) {
        result.success = false;
    }
    return result;
}

bool safeStrToBool(const std::string &s) {
    std::string str = s;
    std::ranges::transform(str, str.begin(),
                           [](const unsigned char c) { return std::tolower(c); });
    return (str == "true" || str == "1" || str == "yes" || str == "on");
}
