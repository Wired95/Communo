#ifndef _NUM_PARSER_H_
#define _NUM_PARSER_H_

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <stdexcept>
#include <type_traits>

using Number = std::variant<
    int8_t,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    float,
    double
>;

enum eNumberTypes : uint8_t
{
    N_INT8,
    N_UINT8,
    N_INT16,
    N_UINT16,
    N_INT32,
    N_UINT32,
    N_INT64,
    N_UINT64,
    N_FLOAT,
    N_DOUBLE,
    N_UNK,
};

// Integer suffix removal
enum class IntegerSuffix
{
    None,
    U,
    L,
    UL,
    LL,
    ULL
};

// Display a Number as a string
std::string number_to_string(const Number& number);

std::pair<std::string_view, IntegerSuffix> remove_suffix(std::string_view s);

// Parse integer according to suffix
Number parse_integer_number(std::string_view input);

// Main parser
Number parse_number(std::string_view s);

// Inspect and outout the numeric value of a Number
void print_number(const Number& number);
eNumberTypes get_number_type(const Number& number);

// Append the right amount of Bytes required by the number type to a string
void append_number(std::string& replyStr, const Number& number);

// Read a buffer from an offset given an appropriate type, and cast its value accordingly
Number read_number(const char* buffer, size_t& offset, eNumberTypes type);

#endif // _NUM_PARSER_H_
