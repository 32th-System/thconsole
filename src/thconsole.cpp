#include "thconsole.h"

extern thconsole console;
lua_State* lua_state;

typedef void stage_end_t();
stage_end_t* stage_end;


int lua_skip_stage(lua_State* L) {
	stage_end();
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
	luaL_dostring(lua_state, cmd);
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

	lua_register(lua_state, "skip_stage", lua_skip_stage);

	return 0;
}
