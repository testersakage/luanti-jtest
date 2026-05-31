// src/mcl/math_common.cpp
#include "math_common.h"
#include <cstring>

namespace mcl_math {

	bool trim_prefix(const char* str, const char* prefix, std::string& out_trimmed)
	{
		size_t prefix_len = std::strlen(prefix);
		if (std::strncmp(str, prefix, prefix_len) == 0) {
			out_trimmed = (str + prefix_len); // ポインタずらしでトリミング
			return true;
		}
		return false;
	}

} // namespace mcl_math
