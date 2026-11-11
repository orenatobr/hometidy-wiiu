// Ordering of Home folder titles: alphabetical by series, then by release year.
#pragma once

#include <string>
#include <vector>

namespace sorter {

/// One line of the series table: titles whose normalized name contains
/// `pattern` belong to `series` and were released in `year`.
struct Rule {
    std::string pattern;  ///< Normalized (see normalize()).
    std::string series;   ///< Normalized series name used as the group key.
    int year = 0;
};

/// Lowercases, folds Latin accents, drops punctuation and a leading article.
///
/// Args:
///     text: UTF-8 text.
///
/// Returns:
///     Space separated words made of [a-z0-9].
std::string normalize(const std::string& text);

/// Parses a rules file.
///
/// Each line is `pattern | series | year`; `#` starts a comment. The first
/// matching rule wins, so list specific patterns before generic ones.
std::vector<Rule> parseRules(const std::string& text);

/// Sort key of a title.
struct Key {
    std::string series;
    int year = 0;
    std::string name;
};

/// Builds the sort key for a display name.
Key makeKey(const std::string& displayName, const std::vector<Rule>& rules);

/// Strict weak ordering: series, then year, then name; digit runs compare as numbers.
bool less(const Key& a, const Key& b);

}  // namespace sorter
