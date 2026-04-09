// cjk_common.cpp
#include "cjk_common.h"
#include <util/string.h> // Luanti 内部の UTF-8 ライブラリ

namespace cjk {

std::vector<uint32_t> utf8_to_codepoints(const std::string &text)
{
    std::wstring w = utf8_to_wide(text);
    std::vector<uint32_t> cps;
    cps.reserve(w.size());
    for (wchar_t c : w)
        cps.push_back((uint32_t)c);
    return cps;
}

std::string codepoints_to_utf8(const std::vector<int> &cps)
{
    std::wstring w;
    w.reserve(cps.size());
    for (int cp : cps)
        w.push_back((wchar_t)cp);
    return wide_to_utf8(w);
}

size_t length(const std::string &text)
{
    std::wstring w = utf8_to_wide(text);
    return w.size();
}

std::string glyph_name(int cp)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "u%04x", cp);
    return std::string(buf);
}

// normalize / category は将来実装
std::string normalize(const std::string &text, const std::string &form)
{
    return text; // TODO: Unicode 正規化は未実装
}

std::string category(int cp)
{
    return "Cn"; // TODO
}

} // namespace cjk
