#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>

#include "safema.h"

/*

HEADER
char[4] magic => "TAB\0"
uint64_t def_size
uint8_t version

TYPES
0 int
1 float
2 char
3 bool false
4 bool true
5 empty string
6 string
7 empty table
8 table

DEF TYPES
int => (uint8_t)<type><data>
float => (uint8_t)<type><data>
char => (uint8_t)<type><data>

string => (uint8_t)<type>(uint64_t)<length><data>
empty_string => <type>

empty_table => <type>
table => (uint8_t)<type>(uint64_t)<length><table>

TABLE
(uint64_t)<index size>
(uint64_t)<index><def>
...
(uint64_t)<index><def>
(uint64_t)<key size>
(uint64_t)<length><key><def>
...
(uint64_t)<length><key><def>


o resultado de {1,2,3, k1 = 0, k2 = 3}:
<TYPE(TABLE)><LENGTH><DATA>

DATA:
(uint64_t)<KEY COUNT>
<DEF(key)><DEF(value)>

*/

typedef enum {
	TAB_INT = 0,
	TAB_FLOAT = 1,
	TAB_CHAR = 2,
	TAB_BOOL_FALSE = 3,
	TAB_BOOL_TRUE = 4,
	TAB_EMPTY_STRING = 5,
	TAB_STRING = 6,
	TAB_EMPTY_TABLE = 7,
	TAB_TABLE = 8,
} tabtype;

struct tab_header{
	uint64_t def_size;
	char magic[4]; // TAB1
	uint8_t version;
};

struct tab_data{
	struct tab_header header;
	uint8_t* data;
	size_t index;
	uint64_t data_size;
};

static int cpu_endian = -1;

void systemEndian(){
    if(cpu_endian != -1) return;

    uint16_t x = 1;
    cpu_endian = (*(uint8_t*)&x) ? 1 : 0;
	return;
}

void reorderBytes(void *ptr, size_t size){
    if (cpu_endian == 0){
        return;
	}

    uint8_t *b = (uint8_t*)ptr;

    for (size_t i = 0; i < size / 2; i++) {
        uint8_t tmp = b[i];
        b[i] = b[size - 1 - i];
        b[size - 1 - i] = tmp;
    } 	
}

static int intToRaw(lua_State *L){
	lua_Integer val = luaL_checkinteger(L, 1);

	lua_Integer tmp = val;
	reorderBytes(&tmp, sizeof(tmp));

	lua_pushlstring(L, (const char *)&tmp, sizeof(tmp));
	return 1;
}

static int numberToRaw(lua_State *L){
	lua_Number val = luaL_checknumber(L, 1);

	lua_Number tmp = val;
	reorderBytes(&tmp, sizeof(tmp));

	lua_pushlstring(L, (const char *)&tmp, sizeof(tmp));
	return 1;
}

static int rawToInt(lua_State *L){
	size_t size = 0;
	const char* str = luaL_checklstring(L, 1, &size);
	if(size != sizeof(lua_Integer)){
		return luaL_error(L,"size of string is different than a lua_Integer [%I]", (lua_Integer)size);
	}
	lua_Integer value;
	memcpy(&value, str, sizeof(value));
	reorderBytes(&value, sizeof(value));
	lua_pushinteger(L, value);
	return 1;
} 

static int rawToNumber(lua_State *L){
	size_t size = 0;
	const char* str = luaL_checklstring(L, 1, &size);
	if(size != sizeof(lua_Number)){
		return luaL_error(L,"size of string is different than a lua_Number [%I]", (lua_Number)size);
	}
	lua_Number value;
	memcpy(&value, str, sizeof(value));
	reorderBytes(&value, sizeof(value));
	lua_pushnumber(L, value);
	return 1;
}

char tabRead(struct tab_data* tab){
	return tab->data[tab->index++];
}

uint64_t tabReadInt(struct tab_data* tab){
	uint8_t bytes[8];
	for(int i = 0; i < 8; i++){
		bytes[i] = tabRead(tab);
	}
	uint64_t tmp;
	memcpy(&tmp, bytes, sizeof(uint64_t));
	reorderBytes(&tmp, sizeof(tmp));
    return tmp;
}

void tabReadString(struct tab_data* tab, char* str, size_t size){
	for(size_t i = 0; i < size; i++){
		str[i] = tabRead(tab);
	}
}

lua_Integer tabReadLuaInteger(struct tab_data* tab){
	uint8_t bytes[8];
	for(int i = 0; i < 8; i++){
		bytes[i] = tabRead(tab);
	}
	lua_Integer tmp;
	memcpy(&tmp, bytes, sizeof(lua_Integer));
	reorderBytes(&tmp, sizeof(tmp));
    return tmp;
}

lua_Number tabReadLuaNumber(struct tab_data* tab){
	uint8_t bytes[8];
	for(int i = 0; i < 8; i++){
		bytes[i] = tabRead(tab);
	}
	lua_Number tmp;
	memcpy(&tmp, bytes, sizeof(lua_Number));
	reorderBytes(&tmp, sizeof(tmp));
    return tmp;
}

/*
	TAB_INT = 0,
	TAB_FLOAT = 1,
	TAB_CHAR = 2,
	TAB_BOOL_FALSE = 3,
	TAB_BOOL_TRUE = 4,
	TAB_EMPTY_STRING = 5,
	TAB_STRING = 6,
	TAB_EMPTY_TABLE = 7,
	TAB_TABLE = 8,
*/

static int tab_unpack(lua_State *L){
	struct tab_data* tab = (struct tab_data*)lua_touserdata(L, -1);
	char type = tabRead(tab);
	switch (type) {
	
	case TAB_INT:
		lua_Integer ivalue = tabReadLuaInteger(tab);
		lua_pop(L, 1);
		lua_pushinteger(L, ivalue);
		return 1;
		break;
	
	case TAB_FLOAT:
		lua_Number nvalue = tabReadLuaNumber(tab);
		lua_pop(L, 1);
		lua_pushnumber(L, nvalue);
		return 1;
		break;
	
	case TAB_CHAR:
		char _c = tabRead(tab);
		lua_pop(L, 1);
		lua_pushlstring(L, &_c, 1);
		return 1;
		break;

	case TAB_BOOL_FALSE:
		lua_pop(L, 1);
		lua_pushboolean(L, 0);
		return 1;
		break;
	
	case TAB_BOOL_TRUE:
		lua_pop(L, 1);
		lua_pushboolean(L, 1);
		return 1;
		break;
	
	case TAB_EMPTY_STRING:
		lua_pop(L, 1);
		lua_pushlstring(L, NULL, 0);
		return 1;
		break;

	case TAB_STRING:
		lua_pop(L, 1);
		uint64_t size = tabReadInt(tab);
		char *str = SMsafeMalloc(size);
		tabReadString(tab, str, size);
		lua_pushlstring(L, str, size);
		free(str);
		return 1;
		break;
	
	case TAB_EMPTY_TABLE:
		lua_pop(L, 1);
		lua_newtable(L);
		return 1;
		break;
	
	case TAB_TABLE: {
        int ud = lua_gettop(L);
        uint64_t key_count = tabReadInt(tab);

        lua_newtable(L);
        int tbl = lua_gettop(L);

        for(uint64_t i = 0; i < key_count; i++) {
            lua_pushvalue(L, ud);
            tab_unpack(L);
            lua_pushvalue(L, ud);
            tab_unpack(L);
            lua_settable(L, tbl);
        }

        lua_remove(L, ud);
        return 1;
    }

	
	default:
		return 0;
		break;
	}
	return 0;
}

static int lua_unpack(lua_State *L){
	size_t data_size;
	uint8_t* data = (uint8_t*)luaL_checklstring(L, 1, &data_size);
	
	if(data_size < 12){
		lua_pushnil(L);
		return 1;
	}

	struct tab_data* tab = (struct tab_data*)lua_newuserdata(L, sizeof(struct tab_data));
	
	size_t expected_def_size = data_size - 13;

	tab->data = data;
	tab->data_size = data_size;
	tab->index = 0;

	tabReadString(tab, tab->header.magic, 4);
	
	if(memcmp(tab->header.magic, "TAB\0", 4)){
		lua_pushnil(L);
		return 1;
	}

	tab->header.magic[0] = 'T';
	tab->header.magic[1] = 'A';
	tab->header.magic[2] = 'B';
	tab->header.magic[3] = 0;

	uint8_t version = tabRead(tab);

	tab->header.version = version;

	uint64_t file_def_size = tabReadInt(tab);

	if(file_def_size != expected_def_size){
		lua_pushnil(L);
		lua_pushinteger(L, file_def_size - expected_def_size);
		return 3;
	}

	tab->header.def_size = file_def_size;

	if(file_def_size > data_size - 12){
		lua_pushnil(L);
		lua_pushnil(L);
		return 3;
	}

	char *def_data = SMsafeMalloc((size_t)file_def_size + 1);
	
	def_data[file_def_size] = 0;

	int stack = tab_unpack(L);

	free(def_data);

	return stack + 1;
}

static int tab_pack(lua_State *L){
	int type = lua_type(L, -1);
	if(type == LUA_TNUMBER){
		if(lua_isinteger(L, -1)){
			uint8_t data[sizeof(lua_Integer) + 1];
			data[0] = (int)TAB_INT;
			lua_Integer value = lua_tointeger(L, -1);
			uint8_t* tmp_ptr = (uint8_t*)&value;
			memcpy(data + 1, tmp_ptr, sizeof(lua_Integer));
			reorderBytes(data + 1, sizeof(lua_Integer));
			lua_pop(L, 1);
			lua_pushlstring(L, (char*)data, sizeof(lua_Integer) + 1);
			return 1;
		}else if(lua_isnumber(L, -1)){
			uint8_t data[sizeof(lua_Number) + 1];
			data[0] = (int)TAB_FLOAT;
			lua_Number value = lua_tonumber(L, -1);
			uint8_t* tmp_ptr = (uint8_t*)&value;
			memcpy(data + 1, tmp_ptr, sizeof(lua_Number));
			reorderBytes(data + 1, sizeof(lua_Number));
			lua_pop(L, 1);
			lua_pushlstring(L, (char*)data, sizeof(lua_Number) + 1);
			return 1;
		}
	}else if(type == LUA_TBOOLEAN){
		uint8_t data[1];
		int boolean = lua_toboolean(L, -1);
		if(boolean){
			data[0] = (int)TAB_BOOL_TRUE;
		}else{
			data[0] = (int)TAB_BOOL_FALSE;
		}
		lua_pop(L, 1);
		lua_pushlstring(L, (char *)data, 1);
		return 1;
	}else if(type == LUA_TSTRING){
		size_t size;
		uint8_t *str = (uint8_t*)luaL_checklstring(L, -1, &size);
		uint8_t *data = SMsafeMalloc(size + 9);

		if(size == 0){
			data[0] = TAB_EMPTY_STRING;
			lua_pop(L, 1);
			lua_pushlstring(L, (char*)data, 1);
			free(data);
			return 1;
		}else if(size == 1){
			data[0] = TAB_CHAR;
			data[1] = str[0];
			lua_pop(L, 1);
			lua_pushlstring(L, (char*)data, 2);
			free(data);
			return 1;
		}

		data[0] = TAB_STRING;
		uint8_t *tmp_ptr = (uint8_t *)&size;
		memcpy(data + 1, tmp_ptr, 8);
		reorderBytes(data + 1, 8);

		memcpy(data + 9, str, size);

		lua_pop(L, 1);
		lua_pushlstring(L, (char *)data, size + 9);

		free(data);
		return 1;
	}else if(type == LUA_TTABLE){
		size_t key_count = 0;

		size_t final_size = 9;
		size_t final_pointer = 9;
		uint8_t *final = SMsafeMalloc(final_size);
		final[0] = (int)TAB_TABLE;

		int tab_index = lua_gettop(L);
		lua_pushnil(L);

		while (lua_next(L, tab_index) != 0) {
			lua_pushvalue(L, -1); // copy value
			tab_pack(L);
			size_t size_data;
			uint8_t* bytes_data = (uint8_t*)lua_tolstring(L, -1, &size_data);
			lua_pop(L, 1); // pop packed value

			lua_pushvalue(L, -2); // copy key
			tab_pack(L);
			size_t size_key;
			uint8_t* bytes_key = (uint8_t*)lua_tolstring(L, -1, &size_key);
			lua_pop(L, 1); // pop packed key

			if((final_pointer + size_key + size_data) >= final_size){
				final_size = (final_pointer + size_key + size_data) + 10;
				final = SMsafeRealloc(final, final_size);
			}

			memcpy(final + final_pointer, bytes_key, size_key);
			final_pointer += size_key;
			memcpy(final + final_pointer, bytes_data, size_data);
			final_pointer += size_data;

			lua_pop(L, 1); // remove value, keep key

			key_count++;
		}

		lua_pop(L, 1);

		if(key_count == 0){
			final[0] = TAB_EMPTY_TABLE;
			lua_pushlstring(L, (char*)final, 1);
			free(final);
			return 1;
		}

		memcpy(final + 1, (uint8_t*)&key_count, 8);
		reorderBytes(final + 1, 8);
		lua_pushlstring(L, (char*)final, final_pointer);

		free(final);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

static int lua_pack(lua_State *L){
	char *file = SMsafeMalloc(13);

	if(lua_type(L, -1) == LUA_TSTRING){
		tab_pack(L);
	}else if(lua_isinteger(L, -1)){
		tab_pack(L);
	}else if(lua_isnumber(L, -1)){
		tab_pack(L);
	}else if(lua_isboolean(L, -1)){ 
		tab_pack(L);
	}else if(lua_istable(L, -1)){
		tab_pack(L);
	}else if(lua_isnil(L, -1)){
		return 0;
	}else{
		return 0;
	}

	uint64_t def_size = 0;
	uint8_t *def = (uint8_t*)lua_tolstring(L, -1, &def_size);
	uint8_t *rawdata = SMsafeMalloc(def_size + 13);
	memcpy(rawdata + 13, def, def_size);
	lua_pop(L, 1);
	
	rawdata[0] = 'T';
	rawdata[1] = 'A';
	rawdata[2] = 'B';
	rawdata[3] = 0;
	rawdata[4] = 255;
	memcpy(rawdata + 5, (uint8_t*)&def_size, sizeof(def_size));
	reorderBytes(rawdata + 5, sizeof(def_size));

	lua_pushlstring(L, (char*)rawdata, def_size + 13);

	free(rawdata);

	return 1;
}

int luaopen_tab(lua_State *L){
	systemEndian();

	luaL_Reg funcs[] = {
		{"intToRaw", intToRaw},
		{"numberToRaw", numberToRaw},
		{"rawToInt", rawToInt},
		{"rawToNumber", rawToNumber},
		{"unpack", lua_unpack},
		{"pack", lua_pack},
		{NULL, NULL}
	};

	luaL_newlib(L, funcs);
	return 1;
}