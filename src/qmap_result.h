#pragma once

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>

namespace qmap {

struct Error {
    std::string message;
    std::size_t offset = 0;
};

template <typename T>
class Result {
public:
    static Result ok(T value)
    {
        return Result(std::move(value));
    }

    static Result fail(Error error)
    {
        return Result(std::move(error));
    }

    bool has_value() const
    {
        return std::holds_alternative<T>(storage_);
    }

    explicit operator bool() const
    {
        return has_value();
    }

    const T& value() const
    {
        assert(has_value());
        return std::get<T>(storage_);
    }

    T& value()
    {
        assert(has_value());
        return std::get<T>(storage_);
    }

    const Error& error() const
    {
        assert(!has_value());
        return std::get<Error>(storage_);
    }

private:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}

    std::variant<T, Error> storage_;
};

} // namespace qmap
