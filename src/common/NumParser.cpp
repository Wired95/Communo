#include "NumParser.h"

// Integer parsing
template<typename T>
bool parse_integer(std::string_view s, T& value, int base = 10)
{
    static_assert(std::is_integral_v<T>);

    const char* begin = s.data();
    const char* end   = s.data() + s.size();

    auto result = std::from_chars(begin, end, value, base);

    return result.ec == std::errc{} &&
           result.ptr == end;
}

// Floating-point parsing
template<typename T>
bool parse_float(std::string_view s, T& value)
{
    std::string tmp(s);

    char* end = nullptr;

    if constexpr (std::is_same_v<T, float>) {
        value = std::strtof(tmp.c_str(), &end);
    }
    else {
        value = std::strtod(tmp.c_str(), &end);
    }

    return end == tmp.c_str() + tmp.size();
}


// Integer suffix removal
std::pair<std::string_view, IntegerSuffix>
remove_suffix(std::string_view s)
{
    auto ends_with = [&](std::string_view suffix) {
        return s.size() >= suffix.size() &&
               s.substr(s.size() - suffix.size()) == suffix;
    };

    // Longest suffixes first.

    if (ends_with("ULL") || ends_with("ull"))
        return {s.substr(0, s.size() - 3), IntegerSuffix::ULL};

    if (ends_with("UL") || ends_with("ul"))
        return {s.substr(0, s.size() - 2), IntegerSuffix::UL};

    if (ends_with("LL") || ends_with("ll"))
        return {s.substr(0, s.size() - 2), IntegerSuffix::LL};

    if (ends_with("U") || ends_with("u"))
        return {s.substr(0, s.size() - 1), IntegerSuffix::U};

    if (ends_with("L") || ends_with("l"))
        return {s.substr(0, s.size() - 1), IntegerSuffix::L};

    return {s, IntegerSuffix::None};
}


// Parse integer according to suffix
Number parse_integer_number(std::string_view input)
{
    auto [s, suffix] = remove_suffix(input);

    // Detect sign.
    bool negative = !s.empty() && s.front() == '-';

    // Determine base.
    int base = 10;

    if (s.size() >= 2 &&
        s[0] == '0' &&
        (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s.remove_prefix(2);

        // Keep the sign separate because from_chars handling
        // of signed hexadecimal values is easy to get wrong.
        if (negative)
            throw std::invalid_argument("negative hexadecimal not supported");
    }

    // Explicit unsigned suffix
    if (suffix == IntegerSuffix::U ||
        suffix == IntegerSuffix::UL ||
        suffix == IntegerSuffix::ULL)
    {
        uint64_t value{};

        if (!parse_integer(s, value, base))
            throw std::out_of_range("integer does not fit uint64_t");

        if (value <= std::numeric_limits<uint8_t>::max())
            return static_cast<uint8_t>(value);

        if (value <= std::numeric_limits<uint16_t>::max())
            return static_cast<uint16_t>(value);

        if (value <= std::numeric_limits<uint32_t>::max())
            return static_cast<uint32_t>(value);

        return value;
    }

    // Explicit long-long
    if (suffix == IntegerSuffix::LL)
    {
        int64_t value{};

        if (!parse_integer(s, value, base))
            throw std::out_of_range("integer does not fit int64_t");

        return value;
    }

    // Explicit long
    if (suffix == IntegerSuffix::L)
    {
        // long is platform-dependent in size.
        // We deliberately represent it as int64_t here.
        int64_t value{};

        if (!parse_integer(s, value, base))
            throw std::out_of_range("integer does not fit int64_t");

        return value;
    }

    // --------------------------------------------------------
    // No suffix: infer the smallest type.
    // --------------------------------------------------------

    if (negative)
    {
        int64_t value{};

        if (!parse_integer(s, value, base))
            throw std::out_of_range("integer does not fit int64_t");

        if (value >= std::numeric_limits<int8_t>::min())
            return static_cast<int8_t>(value);

        if (value >= std::numeric_limits<int16_t>::min())
            return static_cast<int16_t>(value);

        if (value >= std::numeric_limits<int32_t>::min())
            return static_cast<int32_t>(value);

        return value;
    }

    // Positive integer: try unsigned representations.
    uint64_t value{};

    if (!parse_integer(s, value, base))
        throw std::out_of_range("integer does not fit uint64_t");

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<int8_t>::max()))
        return static_cast<int8_t>(value);

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<uint8_t>::max()))
        return static_cast<uint8_t>(value);

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<int16_t>::max()))
        return static_cast<int16_t>(value);

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<uint16_t>::max()))
        return static_cast<uint16_t>(value);

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<int32_t>::max()))
        return static_cast<int32_t>(value);

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<uint32_t>::max()))
        return static_cast<uint32_t>(value);

    if (value <= static_cast<uint64_t>(
                     std::numeric_limits<int64_t>::max()))
        return static_cast<int64_t>(value);

    return value;
}

// Main parser
Number parse_number(std::string_view s)
{
    if (s.empty())
        throw std::invalid_argument("empty number");

    // Explicit float suffix.
    if (s.back() == 'f' || s.back() == 'F')
    {
        float value{};

        if (!parse_float(s.substr(0, s.size() - 1), value))
            throw std::invalid_argument("invalid float");

        return value;
    }

    // Detect floating-point notation.
    bool is_float =
        s.find('.') != std::string_view::npos ||
        s.find('e') != std::string_view::npos ||
        s.find('E') != std::string_view::npos;

    if (is_float)
    {
        double value{};

        if (!parse_float(s, value))
            throw std::invalid_argument("invalid double");

        return value;
    }

    return parse_integer_number(s);
}

void print_number(const Number& number)
{
    std::visit([](auto value)
    {
        using T = decltype(value);

        if constexpr (std::is_same_v<T, int8_t>)
            std::cout << "int8_t: " << +value << '\n';

        else if constexpr (std::is_same_v<T, uint8_t>)
            std::cout << "uint8_t: " << +value << '\n';

        else if constexpr (std::is_same_v<T, int16_t>)
            std::cout << "int16_t: " << value << '\n';

        else if constexpr (std::is_same_v<T, uint16_t>)
            std::cout << "uint16_t: " << value << '\n';

        else if constexpr (std::is_same_v<T, int32_t>)
            std::cout << "int32_t: " << value << '\n';

        else if constexpr (std::is_same_v<T, uint32_t>)
            std::cout << "uint32_t: " << value << '\n';

        else if constexpr (std::is_same_v<T, int64_t>)
            std::cout << "int64_t: " << value << '\n';

        else if constexpr (std::is_same_v<T, uint64_t>)
            std::cout << "uint64_t: " << value << '\n';

        else if constexpr (std::is_same_v<T, float>)
            std::cout << "float: " << value << '\n';

        else if constexpr (std::is_same_v<T, double>)
            std::cout << "double: " << value << '\n';

    }, number);
}

eNumberTypes get_number_type(const Number& number)
{
    std::visit([](auto value)
    {
        using T = decltype(value);

        if constexpr (std::is_same_v<T, int8_t>)
            return N_INT8;
        else if constexpr (std::is_same_v<T, uint8_t>)
            return N_UINT8;
        else if constexpr (std::is_same_v<T, int16_t>)
            return N_INT16;
        else if constexpr (std::is_same_v<T, uint16_t>)
            return N_UINT16;
        else if constexpr (std::is_same_v<T, int32_t>)
            return N_INT32;
        else if constexpr (std::is_same_v<T, uint32_t>)
            return N_UINT32;
        else if constexpr (std::is_same_v<T, int64_t>)
            return N_INT64;
        else if constexpr (std::is_same_v<T, uint64_t>)
           return N_UINT64;
        else if constexpr (std::is_same_v<T, float>)
            return N_FLOAT;
        else if constexpr (std::is_same_v<T, double>)
            return N_DOUBLE;
    }, number);

    return N_UNK;
}