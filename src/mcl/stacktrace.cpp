// src/mcl/stacktrace.cpp
#include "stacktrace.h"

namespace stacktrace {

int find_table_by_method(lua_State *L, const char *method_name)
{
	int top = lua_gettop(L);
	for (int i = 1; i <= top; i++) {
		if (lua_istable(L, i)) {
			// 指定されたメソッド（例: "get_name" や "decompose_AABBs"）の存在をスキャン！
			lua_getfield(L, i, method_name);
			if (lua_isfunction(L, -1)) {
				lua_pop(L, 1);
				return i; // 1発特定ロックオン！
			}
			lua_pop(L, 1);
		}
	}
	return 0; // 見つからなかった場合は0を返す
}

} // namespace stacktrace
