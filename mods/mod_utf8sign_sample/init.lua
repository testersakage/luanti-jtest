-- mod_utf8sign_sample/init.lua
local S = minetest.get_translator("mod_utf8sign_sample")

local modpath = minetest.get_modpath(minetest.get_current_modname())
-- --- 1. C++エンジンへの物差し通知 ---
if minetest.utf8sign then
	minetest.utf8sign.set_config({
		st = {
--[[
-- for PixelMplus 12px
			st_line_height = 14,
			st_max_lines = 4,
			st_char_w_han = 6,
			st_char_w_zen = 12,
			st_atlas_path = modpath .. "/textures/pixelmplus_12/unicode_page_%02x.png",
			st_grid_columns = 32,
			st_grid_size = 14,
			st_alpha_reverse = false,
			uax_half_switch = false
]]
-- for signs_lib Unifont
			st_char_w_han = 8,
			st_char_w_zen = 16,
			st_line_height = 16,
			st_max_lines = 4,
			st_atlas_path = modpath .. "/textures/unifont/signs_lib_uni%02x.png",
			st_grid_columns = 16,
			st_grid_size = 16,
			st_alpha_reverse = true,
			uax_half_switch = true

		}
	})
end

--定数
local ST_SIGN_WIDTH = 115 -- 看板文字用テクスチャのサイズ（幅）

-- sound setting
local sounds = {}
if minetest.get_modpath("default") then
	sounds = default.node_sound_wood_defaults()
elseif minetest.get_modpath("mcl_sounds") then
	sounds = mcl_sounds.node_sound_wood_defaults()
end

-- --- 2. 補助関数の定義 ---

-- 看板の向き(param2)から、エンティティの座標と回転を計算する
local function get_sign_offsets_and_rot(param2)
	local off = 0.435
	local data = {
		[0] = {x= 0,    y= off,   z= 0,    yaw= 0,          pitch= math.pi/2},  -- 天井
		[1] = {x= 0,    y=-off,   z= 0,    yaw= 0,          pitch=-math.pi/2},  -- 地面
		[2] = {x= off,  y= 0,     z= 0,    yaw= math.pi/2,  pitch= 0},          -- 東
		[3] = {x=-off,  y= 0,     z= 0,    yaw=-math.pi/2,  pitch= 0},          -- 西
		[4] = {x= 0,    y= 0,     z= off,  yaw= 0,          pitch= 0},          -- 北
		[5] = {x= 0,    y= 0,     z=-off,  yaw= math.pi,    pitch= 0},          -- 南
	}
	return data[param2] or {x=0, y=0, z=0, yaw=0, pitch=0}
end

-- 文字更新用の関数
local function update_sign_visual(pos, text)
	local tex = "utf8_blank.png"
	if text ~= "" then
		-- 115x82キャンバス、左余白15、上余白14、黒色
--		tex = "[utf8combine:" .. ST_SIGN_WIDTH .. "x82:15,14@000000=" .. text .. "]"
		tex = "[utf8combine:" .. ST_SIGN_WIDTH .. "x82:4,4@000000=" .. text .. "]"
		print("DEBUG_LUA_TEX: " .. tex) -- これをターミナルに表示させる
	end

	local objects = minetest.get_objects_inside_radius(pos, 0.5)
	for _, obj in ipairs(objects) do
		local ent = obj:get_luaentity()
		if ent and ent.name == "mod_utf8sign_sample:text_entity" then
			obj:set_properties({textures = {tex}})
		end
	end
end

-- --- 3. 文字表示プレート(Entity)の定義 ---
minetest.register_entity("mod_utf8sign_sample:text_entity", {
	visual = "upright_sprite",
	visual_size = {x = 0.875, y = 0.625}, 
	textures = {"utf8_blank.png"}, -- デフォルトは透明
	physical = false,
	pointable = false,
	is_visible = true,
	static_save = true,
	on_activate = function(self, staticdata)
		local pos = self.object:get_pos()
		local meta = minetest.get_meta(pos)
		local text = meta:get_string("text")
		if text ~= "" then
			minetest.after(0.2, function()
				update_sign_visual(pos, text)
			end)
		end
	end,
})

-- --- 4. 看板登録用ヘルパー ---
local function register_utf8_sign(material, desc, groups, sounds)
	-- soundsが渡されていない（nil）場合の安全装置
	sounds = sounds or {}
	minetest.register_node("mod_utf8sign_sample:sign_" .. material, {
		description = desc,
		drawtype = "nodebox",
		tiles = {"utf8_sign_wall_" .. material .. ".png"},
		inventory_image = "utf8_sign_" .. material .. ".png",
		wield_image = "utf8_sign_" .. material .. ".png",
		paramtype = "light",
		paramtype2 = "wallmounted",
		sunlight_propagates = true,
		is_ground_content = false,
		walkable = false,
		use_texture_alpha = "opaque",
		node_box = {
			type = "wallmounted",
			wall_top    = {-0.4375, 0.4375, -0.3125, 0.4375, 0.5, 0.3125},
			wall_bottom = {-0.4375, -0.5, -0.3125, 0.4375, -0.4375, 0.3125},
			wall_side   = {-0.5, -0.3125, -0.4375, -0.4375, 0.3125, 0.4375},
		},
		-- 当たり判定をモデルの形（node_box）に合わせる
		selection_box = {
			type = "wallmounted",
		},
		groups = groups,
		legacy_wallmounted = true,
		sounds = sounds,

		on_construct = function(pos)
			local node = minetest.get_node(pos)
			local d = get_sign_offsets_and_rot(node.param2)
			local obj = minetest.add_entity({x = pos.x + d.x, y = pos.y + d.y, z = pos.z + d.z}, "mod_utf8sign_sample:text_entity")
			if obj then obj:set_rotation({x = d.pitch, y = d.yaw, z = 0}) end

			local formspec = 
				"formspec_version[6]" ..
				"size[5.5,5.5]" .. -- ウィンドウ全体を少しコンパクトに
				"real_coordinates[true]" ..
				-- ラベル：上端から少し余裕を持たせる
				"label[0.5,0.7;看板に書き込む内容 (4行まで):]" ..
				-- 入力欄：画像通りの「どっしり」した広さ
				"textarea[1.0,1.2;3.5,2.0;text;;${text}]" ..
				-- 決定ボタン：下側にゆったり配置
				"button_exit[1.5,4.2;2.5,0.8;save;決定]"
			minetest.get_meta(pos):set_string("formspec", formspec)
			minetest.get_meta(pos):set_string("text", "")

		end,

		on_destruct = function(pos)
			-- 範囲を 0.5 から 0.8 程度に広げて確実に捕まえる
			local objects = minetest.get_objects_inside_radius(pos, 0.8)
			for _, obj in ipairs(objects) do
				local ent = obj:get_luaentity()
				-- 自分のModのエンティティなら問答無用で削除
				if ent and ent.name == "mod_utf8sign_sample:text_entity" then
					obj:remove()
				end
			end
		end,

		on_receive_fields = function(pos, formname, fields, sender)
			-- 1. 「決定(save)」ボタンが押された時だけ処理する
			-- Esc で閉じた時は fields.save が nil なので、ここで終了する
			if not fields.save then
				return
			end
--			if not fields.quit then return end

			local text = fields.text or ""
			local meta = minetest.get_meta(pos)
			meta:set_string("text", text)
			meta:set_string("infotext", '"' .. text .. '"')
			update_sign_visual(pos, text)
		end,
	})
end

-- --- 音の定義を「安全」にする ---
local wood_sounds = {}
local steel_sounds = {}

if minetest.get_modpath("default") then
	-- Minetest Game (MTG) 環境
	wood_sounds = default.node_sound_wood_defaults()
	steel_sounds = default.node_sound_metal_defaults()
elseif minetest.get_modpath("mcl_sounds") then
	-- Mineclonia / MineClone2 環境
	wood_sounds = mcl_sounds.node_sound_wood_defaults()
	steel_sounds = mcl_sounds.node_sound_metal_defaults()
end

-- 木の看板
register_utf8_sign("wood", S("Wooden Sign (UTF-8 Atlas)"), 
	{choppy = 2, attached_node = 1, flammable = 2, oddly_breakable_by_hand = 3},
	wood_sounds)

-- 鉄の看板
register_utf8_sign("steel", S("Steel Sign (UTF-8 Atlas)"), 
	{cracky = 2, attached_node = 1},
	steel_sounds)
