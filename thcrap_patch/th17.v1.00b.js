{
	"options": {
		"stage_end_func": {
			"type": "u32",
			"val": "Rx32060"
		}
	},
	"breakpoints": {
		"thconsole_ui_init": {
			"cavesize": 5,
			"addr": "Rx40827",
			"hwnd": "[Rx1226c0]",
			"d3ddevice": "[Rxb5ae8]",
			"texture": "[[[[Rx109A20]+0x1860100+0x0]+0x124]+0x0]",
			"ignore": false
		},
		
		"thconsole_ui_draw": {
			"cavesize": 5,
			"addr": "Rx62108",
			"ignore": false
		},
		"thconsole_toggle": {
			"cavesize": 5,
			"addr": "Rx4D95",
			"toggle": "eax & 0x800000"
		},
		"thconsole_lock_input": {
			"cavesize": 5,
			"addr": "Rx40f89"
		},
		"thconsole_skip_code#invincibility": {
			"cave_exec": false,
			"cavesize": 6,
			"eip_jump_dist": "0x1B3",
			"addr": "Rx49420",
			"lua_cond": "invincibility"
		}
	}
}