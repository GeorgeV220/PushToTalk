#ifndef CONVERSION_H
#define CONVERSION_H

#include <string>
#include <cstdint>

struct IntConversionResult {
    int value;
    bool success;

    IntConversionResult() : value(-1), success(false) {
    }
};

struct LongConversionResult {
    long value;
    bool success;

    LongConversionResult() : value(-1), success(false) {
    }
};

struct FloatConversionResult {
    float value;
    bool success;

    FloatConversionResult() : value(0.0f), success(false) {
    }
};

struct UInt16ConversionResult {
    uint16_t value;
    bool success;

    UInt16ConversionResult() : value(0), success(false) {
    }
};

struct UInt32ConversionResult {
    uint32_t value;
    bool success;

    UInt32ConversionResult() : value(0), success(false) {
    }
};

struct StringConversionResult {
    std::string str;
    bool success;

    StringConversionResult() : str(""), success(false) {
    }
};

IntConversionResult safeStrToInt(const std::string &str);

LongConversionResult safeStrToLong(const std::string &str);

FloatConversionResult safeStrToFloat(const std::string &str);

UInt16ConversionResult safeStrToUInt16(const std::string &str);

UInt32ConversionResult safeStrToUInt32(const std::string &str);

StringConversionResult safeIntToStr(int value);

StringConversionResult safeLongToStr(long value);

StringConversionResult safeUInt16ToStr(uint16_t value);

StringConversionResult safeUInt32ToStr(uint32_t value);

StringConversionResult safeFloatToStr(float value);

bool safeStrToBool(const std::string &str);

#endif // CONVERSION_H
