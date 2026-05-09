#include "l_utf8_sign_client.h"
#include "common/l_utf8sign.h"

namespace l_utf8_sign_client {
	void Initialize(lua_State *L, int top) {
		// common の初期化を呼び出し、クライアント側 Lua 環境に API を登録
		l_utf8sign::Initialize(L, top);
	}
}
