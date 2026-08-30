#ifndef LE_RUNTIME_CORE_TEXT_HPP
#define LE_RUNTIME_CORE_TEXT_HPP

#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace le::core {

class ByteOffset {
  public:
    constexpr explicit ByteOffset(std::uint64_t value = 0) : value_(value) {}
    [[nodiscard]] constexpr std::uint64_t value() const { return value_; }
    friend constexpr bool operator==(ByteOffset, ByteOffset) = default;
    friend constexpr auto operator<=>(ByteOffset, ByteOffset) = default;

  private:
    std::uint64_t value_;
};

class TextSpan {
  public:
    constexpr TextSpan(ByteOffset begin, ByteOffset end) : begin_(begin), end_(end) {
        if (end < begin) {
            throw std::logic_error("text span end precedes begin");
        }
    }
    [[nodiscard]] constexpr ByteOffset begin() const { return begin_; }
    [[nodiscard]] constexpr ByteOffset end() const { return end_; }
    [[nodiscard]] constexpr bool empty() const { return begin_ == end_; }

  private:
    ByteOffset begin_;
    ByteOffset end_;
};

enum class SpanBoundary : std::uint8_t {
    code_point,
    grapheme,
};

class InvalidUtf8 final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct Grapheme {
    TextSpan span;
    std::int32_t first_code_point;
};

class Text {
  public:
    explicit Text(std::string_view bytes);

    [[nodiscard]] std::string_view bytes() const { return bytes_; }
    [[nodiscard]] const std::vector<Grapheme>& graphemes() const { return graphemes_; }
    [[nodiscard]] bool is_byte_boundary(ByteOffset offset) const;
    [[nodiscard]] bool is_grapheme_boundary(ByteOffset offset) const;
    [[nodiscard]] bool is_valid_span(TextSpan span, SpanBoundary boundary) const;
    [[nodiscard]] std::vector<const Grapheme*> graphemes_in(TextSpan span) const;

  private:
    struct CodePoint {
        std::int32_t value;
        ByteOffset begin;
    };

    std::string_view bytes_;
    std::vector<CodePoint> code_points_;
    std::vector<Grapheme> graphemes_;
};

} // namespace le::core

#endif
