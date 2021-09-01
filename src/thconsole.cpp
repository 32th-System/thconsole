#include "thconsole.h"

extern thconsole console;
lua_State* lua_state;

typedef void stage_end_t();
stage_end_t* stage_end;

patch_value_type_t parse_tname(const char* tname) {
	switch (const uint8_t type_char = tname[0] | 0x20) {
		case 'i': {
			switch (strtol(tname + 1, nullptr, 10)) {
			case 8:
				return PVT_SBYTE;
			case 16:
				return PVT_SWORD;
			case 32:
				return PVT_SDWORD;
			case 64:
				return PVT_SQWORD;
			}
			break;
		}
		case 'b': case 'u': {
			switch (strtol(tname + 1, nullptr, 10)) {
			case 8:
				return PVT_BYTE;
			case 16:
				return PVT_WORD;
			case 32:
				return PVT_DWORD;
			case 64:
				return PVT_QWORD;
			}
			break;
		}
		case 'f': {
			switch (strtol(tname + 1, nullptr, 10)) {
			case 32:
				return PVT_FLOAT;
			case 64:
				return PVT_DOUBLE;
			case 80:
				return PVT_LONGDOUBLE;
			}
			break;
		}
	}
	return PVT_UNKNOWN;
}

int lua_skip_stage(lua_State* L) {
	stage_end();
	return 0;
}

template<typename T>
T __forceinline peek(uintptr_t addr) {
	return *(T*)addr;
}

template<typename T>
void __forceinline poke(uintptr_t addr, T val) {
	*(T*)addr = val;
}

int lua_peek(lua_State* L) {
	if (lua_gettop(L) != 2) {
		lua_pushstring(L, "peek: wrong number of arguments");
		return 1;
	}

	uintptr_t addr = lua_tointeger(L, 1);

	if (addr <= 0xFFFF || addr > 0x7FFFFFFF) { // Windows be like
		lua_pushstring(L, "peek: address in invalid range");
		return 1;
	}

	const char* tname = lua_tostring(L, 2); 

	switch(parse_tname(tname)) {
	case PVT_UNKNOWN:
		{
			std::string error = "peek: unknown type ";
			error.append(tname);
			lua_pushstring(L, error.c_str());
		}
		break;
	case PVT_BYTE:
		lua_pushinteger(L, peek<uint8_t>(addr));
		break;
	case PVT_WORD:
		lua_pushinteger(L, peek<uint16_t>(addr));
		break;
	case PVT_DWORD:
		lua_pushinteger(L, peek<uint32_t>(addr));
		break;
	case PVT_QWORD:
		lua_pushinteger(L, peek<uint64_t>(addr));
		break;
	case PVT_SBYTE:
		lua_pushinteger(L, peek<int8_t>(addr));
		break;
	case PVT_SWORD:
		lua_pushinteger(L, peek<int16_t>(addr));
		break;
	case PVT_SDWORD:
		lua_pushinteger(L, peek<int32_t>(addr));
		break;
	case PVT_SQWORD:
		lua_pushinteger(L, peek<int64_t>(addr));
		break;
	case PVT_FLOAT:
		lua_pushnumber(L, peek<float>(addr));
		break;
	case PVT_DOUBLE:
		lua_pushnumber(L, peek<double>(addr));
		break;
	case PVT_LONGDOUBLE:
		lua_pushnumber(L, peek<LongDouble80>(addr));
		break;
	}
	return 1;
}

int lua_poke(lua_State * L) {
	if (lua_gettop(L) != 3) {
		lua_pushstring(L, "poke: wrong number of arguments");
		return 1;
	}

	uintptr_t addr = lua_tointeger(L, 1);

	if (addr <= 0xFFFF || addr > 0x7FFFFFFF) { // Windows be like
		lua_pushstring(L, "poke: address in invalid range");
		return 1;
	}

	const char* tname = lua_tostring(L, 2);
	switch (parse_tname(tname)) {
	case PVT_UNKNOWN:
	{
		std::string error = "peek: unknown type ";
		error.append(tname);
		lua_pushstring(L, error.c_str());
	}
	break;
	case PVT_BYTE:
		poke(addr, (uint8_t)lua_tointeger(L, 3));
		break;
	case PVT_WORD:
		poke(addr, (uint16_t)lua_tointeger(L, 3));
		break;
	case PVT_DWORD:
		poke(addr, (uint32_t)lua_tointeger(L, 3));
		break;
	case PVT_QWORD:
		poke(addr, (uint64_t)lua_tointeger(L, 3));
		break;
	case PVT_SBYTE:
		poke(addr, (int8_t)lua_tointeger(L, 3));
		break;
	case PVT_SWORD:
		poke(addr, (int16_t)lua_tointeger(L, 3));
		break;
	case PVT_SDWORD:
		poke(addr, (int32_t)lua_tointeger(L, 3));
		break;
	case PVT_SQWORD:
		poke(addr, (int64_t)lua_tointeger(L, 3));
		break;
	case PVT_FLOAT:
		poke(addr, (float)lua_tonumber(L, 3));
		break;
	case PVT_DOUBLE:
		poke(addr, (double)lua_tonumber(L, 3));
		break;
	case PVT_LONGDOUBLE:
		poke(addr, (LongDouble80)lua_tonumber(L, 3));
		break;
	}
	return 0;
}

int BP_thconsole_skip_code(x86_reg_t* regs, json_t* bp_info) {
	lua_getglobal(lua_state, json_object_get_string(bp_info, "lua_cond"));
	bool cond = lua_toboolean(lua_state, -1);
	lua_pop(lua_state, 1);
	if (!cond) {
		return 1;
	}

	regs->retaddr += json_object_get_immediate(bp_info, regs, "eip_jump_dist");
	regs->esp += json_object_get_immediate(bp_info, regs, "stack_clear_size");

	json_t* obj = NULL;

#define SET_REG(r) \
	if(obj = json_object_get(bp_info, #r)) \
		regs->r = json_immediate_value(obj, regs)

	SET_REG(eax);
	SET_REG(ebx);
	SET_REG(ecx);
	SET_REG(edx);
	SET_REG(ebp);
	SET_REG(esi);
	SET_REG(edi);
	SET_REG(eflags);

#undef SET_REG

	return breakpoint_cave_exec_flag(bp_info);
}

const char* exec_command(const char* cmd) {
	std::string cmd_real = "return ";
	cmd_real.append(cmd);

	luaL_dostring(lua_state, cmd_real.c_str());
	int stack_size = lua_gettop(lua_state);

	if (stack_size) {
		std::string result = "";
		for (int i = 0; i < stack_size; i++) {
			const char* s = lua_tostring(lua_state, i + 1);
			if (s) {
				result.append(s);
				result.push_back('\n');
			}
		}
		lua_settop(lua_state, 0);
		return strdup(result.c_str());
	} else {
		return NULL;
	}
}

int TH_STDCALL thcrap_plugin_init() {
	if (stack_remove_if_unneeded("base_tsa") || !patch_opt_get("patch:thconsole")) {
		return 1;
	}

	patch_val_t* stage_end_func = patch_opt_get("stage_end_func");
	if (stage_end_func->type == PVT_DWORD) {
		stage_end = (stage_end_t*)stage_end_func->i;
	}

	lua_state = luaL_newstate();
	luaL_openlibs(lua_state);

	lua_register(lua_state, "peek", lua_peek);
	lua_register(lua_state, "poke", lua_poke);

	lua_register(lua_state, "skip_stage", lua_skip_stage);

	return 0;
}
