#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Concept: T must support operator<< to std::ostream, since Logged<T>
// logs every access/change/compare by printing the value.
template <typename T>
concept Streamable = requires(std::ostream& os, const T& t) {
    { os << t } -> std::same_as<std::ostream&>;
};

// Generic "call and log" wrapper. Prints [CALL] <name>, then invokes func,
// forwarding its return value (or returning void) transparently.
template <typename Func>
auto logCall(std::string_view name, Func&& func)
{
    std::cout << "[CALL] " << name << '\n';

    if constexpr (std::is_void_v<std::invoke_result_t<Func>>)
    {
        std::forward<Func>(func)();
    }
    else
    {
        return std::forward<Func>(func)();
    }
}

template <Streamable T>
class Logged
{
private:
    std::string name;
    T value{};

    void logAccess() const
    {
        if (verbose && av)
            std::cout << "[ACCESS] " << name << ": " << value << '\n';
    }

    void logChange(const T& oldValue, const T& newValue) const
    {
        if (verbose && chv)
            std::cout << "[CHANGE] " << name
                      << ": " << oldValue
                      << " -> " << newValue << '\n';
    }

    void logCompare(const T& otherValue, const std::string& op) const
    {
        if (verbose && cv)
            std::cout << "[COMPARE] " << name
                      << " " << op
                      << " " << otherValue << '\n';
    }

    // Shared implementation for every "read old, mutate, log" compound-assign op.
    template <typename BinaryOp>
    Logged& applyAndLog(const T& rhs, BinaryOp&& op)
    {
        T old = value;
        value = op(value, rhs);
        logChange(old, value);
        return *this;
    }

public:
    // general verboseness
    bool verbose = true;
    // access verboseness
    bool av = true;
    // compare verboseness
    bool cv = true;
    // change verboseness
    bool chv = true;

    Logged(std::string name)
        : name(std::move(name))
    {
    }

    Logged(std::string name, const T& value)
        : name(std::move(name)),
          value(value)
    {
    }

    void access() const
    {
        logAccess();
    }

    void operator()() const
    {
        logAccess();
    }

    const T& get() const
    {
        logAccess();
        return value;
    }

    const std::string& getName() const
    {
        return name;
    }

    Logged& operator=(const T& newValue)
    {
        logChange(value, newValue);
        value = newValue;
        return *this;
    }

    Logged& operator+=(const T& rhs) { return applyAndLog(rhs, std::plus<>{}); }
    Logged& operator-=(const T& rhs) { return applyAndLog(rhs, std::minus<>{}); }
    Logged& operator*=(const T& rhs) { return applyAndLog(rhs, std::multiplies<>{}); }
    Logged& operator/=(const T& rhs) { return applyAndLog(rhs, std::divides<>{}); }
    Logged& operator%=(const T& rhs) { return applyAndLog(rhs, std::modulus<>{}); }

    Logged& operator&=(const T& rhs)  requires std::integral<T> { return applyAndLog(rhs, std::bit_and<>{}); }
    Logged& operator|=(const T& rhs)  requires std::integral<T> { return applyAndLog(rhs, std::bit_or<>{}); }
    Logged& operator^=(const T& rhs)  requires std::integral<T> { return applyAndLog(rhs, std::bit_xor<>{}); }
    Logged& operator<<=(const T& rhs) requires std::integral<T>
    {
        return applyAndLog(rhs, [](const T& a, const T& b) { return a << b; });
    }
    Logged& operator>>=(const T& rhs) requires std::integral<T>
    {
        return applyAndLog(rhs, [](const T& a, const T& b) { return a >> b; });
    }

    Logged& operator++()
    {
        T old = value;
        ++value;
        logChange(old, value);
        return *this;
    }

    T operator++(int)
    {
        T old = value;
        ++value;
        logChange(old, value);
        return old;
    }

    Logged& operator--()
    {
        T old = value;
        --value;
        logChange(old, value);
        return *this;
    }

    T operator--(int)
    {
        T old = value;
        --value;
        logChange(old, value);
        return old;
    }

    // Explicit rather than implicit: avoids silent/ambiguous conversions and
    // surprise double-logging when comparing two Logged<T> objects.
    explicit operator T() const
    {
        logAccess();
        return value;
    }

    T operator+() const { logAccess(); return +value; }
    T operator-() const { logAccess(); return -value; }
    T operator!() const { logAccess(); return !value; }
    T operator~() const requires std::integral<T> { logAccess(); return ~value; }

    bool operator==(const T& rhs) const
    {
        logCompare(rhs, "==");
        return value == rhs;
    }

    bool operator!=(const T& rhs) const
    {
        logCompare(rhs, "!=");
        return value != rhs;
    }

    bool operator<(const T& rhs) const
    {
        logCompare(rhs, "<");
        return value < rhs;
    }

    bool operator<=(const T& rhs) const
    {
        logCompare(rhs, "<=");
        return value <= rhs;
    }

    bool operator>(const T& rhs) const
    {
        logCompare(rhs, ">");
        return value > rhs;
    }

    bool operator>=(const T& rhs) const
    {
        logCompare(rhs, ">=");
        return value >= rhs;
    }

    // Symmetric overloads so two Logged<T> objects can be compared directly
    // without relying on the (now explicit) conversion operator.
    bool operator==(const Logged& rhs) const { return *this == rhs.value; }
    bool operator!=(const Logged& rhs) const { return *this != rhs.value; }
    bool operator<(const Logged& rhs)  const { return *this < rhs.value; }
    bool operator<=(const Logged& rhs) const { return *this <= rhs.value; }
    bool operator>(const Logged& rhs)  const { return *this > rhs.value; }
    bool operator>=(const Logged& rhs) const { return *this >= rhs.value; }

    Logged& operator+=(const Logged& rhs) { return *this += rhs.value; }
    Logged& operator-=(const Logged& rhs) { return *this -= rhs.value; }
    Logged& operator*=(const Logged& rhs) { return *this *= rhs.value; }
    Logged& operator/=(const Logged& rhs) { return *this /= rhs.value; }
    Logged& operator%=(const Logged& rhs) { return *this %= rhs.value; }

    // Direct printing without needing to convert to T first.
    friend std::ostream& operator<<(std::ostream& os, const Logged& logged)
    {
        os << logged.value;
        return os;
    }
};