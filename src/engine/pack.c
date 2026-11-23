#include "pack.h"

#include "cJSON.h"
#include "lauxlib.h"
#include "lualib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static const char *const file_keys[] = {
    "script", "soldier", "sword", "shield", "sprite", "banner", "strike", "horn", "crowd"
};

typedef struct {
    size_t bytes;
    unsigned remaining;
} ScriptLimit;

static bool id_character(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
}

bool fodder_valid_id(const char *id)
{
    size_t length = strlen(id);
    if (length == 0 || length >= FODDER_PACK_ID)
        return false;
    for (; *id; ++id) {
        if (!id_character(*id))
            return false;
    }
    return true;
}

bool fodder_valid_path(const char *path)
{
    size_t length = strlen(path);
    if (length == 0 || length >= FODDER_FILE || *path == '/' || path[length - 1] == '/')
        return false;
    bool start = true;
    for (; *path; ++path) {
        if (*path == '/') {
            if (start)
                return false;
            start = true;
        } else {
            if (!id_character(*path) && (*path != '.' || start))
                return false;
            start = false;
        }
    }
    return true;
}

bool fodder_path_join(char *destination, size_t length, const char *root, const char *file)
{
    int used = snprintf(destination, length, "%s/%s", root, file);
    return used >= 0 && (size_t)used < length;
}

FILE *fodder_open_read(const char *path)
{
#ifdef _WIN32
    wchar_t wide[FODDER_PATH];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, FODDER_PATH) == 0)
        return NULL;
    return _wfopen(wide, L"rb");
#else
    return fopen(path, "rb");
#endif
}

char *fodder_read(const char *path, size_t limit, size_t *length)
{
    FILE *stream = fodder_open_read(path);
    if (!stream)
        return NULL;
    char *data = NULL;
    if (fseek(stream, 0, SEEK_END) == 0) {
        long size = ftell(stream);
        if (size >= 0 && (unsigned long)size <= limit && fseek(stream, 0, SEEK_SET) == 0) {
            data = malloc((size_t)size + 1);
            if (data && fread(data, 1, (size_t)size, stream) == (size_t)size) {
                *length = (size_t)size;
                data[size] = '\0';
            } else {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(stream);
    return data;
}

bool fodder_pack_file(const FodderPack *pack, FodderPackFile file, char path[FODDER_PATH])
{
    return fodder_path_join(path, FODDER_PATH, pack->root, pack->files[file]);
}

static bool copy_label(char *target, size_t capacity, const char *value, size_t size)
{
    if (!value || !size || size >= capacity || memchr(value, 0, size))
        return false;
    for (size_t i = 0; i < size; ++i) {
        if ((unsigned char)value[i] < 32)
            return false;
    }
    memcpy(target, value, size);
    target[size] = '\0';
    return true;
}

static bool read_manifest(FodderPack *pack)
{
    char path[FODDER_PATH];
    size_t length;
    if (!fodder_path_join(path, sizeof(path), pack->root, "pack.json"))
        return false;
    char *text = fodder_read(path, 32768, &length);
    if (!text)
        return false;
    cJSON *manifest = cJSON_ParseWithLengthOpts(text, length + 1, NULL, 1);
    free(text);
    const cJSON *api = cJSON_GetObjectItemCaseSensitive(manifest, "api");
    const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(manifest, "id"));
    const char *title = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(manifest, "title"));
    const cJSON *files = cJSON_GetObjectItemCaseSensitive(manifest, "files");
    bool valid = cJSON_IsNumber(api) && api->valuedouble == 1.0 &&
                 id && fodder_valid_id(id) && title &&
                 copy_label(pack->title, sizeof(pack->title), title, strlen(title));
    if (valid)
        memcpy(pack->id, id, strlen(id) + 1);
    for (unsigned i = 0; valid && i < FODDER_PACK_FILES; ++i) {
        const char *path_value = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(files, file_keys[i]));
        valid = path_value && fodder_valid_path(path_value);
        if (valid)
            memcpy(pack->files[i], path_value, strlen(path_value) + 1);
    }
    cJSON_Delete(manifest);
    return valid;
}

static void *limit_alloc(void *data, void *pointer, size_t old_size, size_t new_size)
{
    const size_t budget = 3u * 1024 * 1024;
    ScriptLimit *limit = data;
    size_t existing = pointer ? old_size : 0;
    if (new_size == 0) {
        free(pointer);
        limit->bytes -= existing;
        return NULL;
    }
    if (new_size > budget - (limit->bytes - existing))
        return NULL;
    void *result = realloc(pointer, new_size);
    if (result)
        limit->bytes = limit->bytes - existing + new_size;
    return result;
}

static void check_script_budget(lua_State *lua, lua_Debug *record)
{
    (void)record;
    void *data;
    lua_getallocf(lua, &data);
    ScriptLimit *limit = data;
    if (!limit->remaining)
        luaL_error(lua, "Lua instruction budget exceeded");
    --limit->remaining;
}

static int run_script(lua_State *lua, int arguments, int results)
{
    void *data;
    lua_getallocf(lua, &data);
    ((ScriptLimit *)data)->remaining = 400;
    lua_sethook(lua, check_script_budget, LUA_MASKCOUNT, 1000);
    int status = lua_pcall(lua, arguments, results, 0);
    lua_sethook(lua, NULL, 0, 0);
    return status;
}

static lua_State *new_script(void)
{
    ScriptLimit *limit = calloc(1, sizeof(*limit));
    if (!limit)
        return NULL;
    lua_State *lua = lua_newstate(limit_alloc, limit);
    if (!lua) {
        free(limit);
        return NULL;
    }
    luaL_requiref(lua, "_G", luaopen_base, 1);
    luaL_requiref(lua, "table", luaopen_table, 1);
    luaL_requiref(lua, "string", luaopen_string, 1);
    luaL_requiref(lua, "math", luaopen_math, 1);
    lua_settop(lua, 0);
    const char *const blocked[] = {
        "dofile", "loadfile", "load", "print", "warn", "collectgarbage",
        "pcall", "xpcall", "setmetatable", "getmetatable"
    };
    for (unsigned i = 0; i < sizeof(blocked) / sizeof(blocked[0]); ++i) {
        lua_pushnil(lua);
        lua_setglobal(lua, blocked[i]);
    }
    lua_getglobal(lua, "math");
    lua_pushnil(lua);
    lua_setfield(lua, -2, "random");
    lua_pushnil(lua);
    lua_setfield(lua, -2, "randomseed");
    lua_pop(lua, 1);
    return lua;
}

static void field(lua_State *lua, int table, const char *key)
{
    table = lua_absindex(lua, table);
    lua_pushstring(lua, key);
    lua_rawget(lua, table);
}

static float number(lua_State *lua, int table, const char *key, float min, float max, bool *valid)
{
    field(lua, table, key);
    float value = (float)lua_tonumber(lua, -1);
    if (lua_type(lua, -1) != LUA_TNUMBER || !isfinite(value) || value < min || value > max) {
        *valid = false;
        value = min;
    }
    lua_pop(lua, 1);
    return value;
}

static bool label(lua_State *lua, int table, const char *key, char *target, size_t capacity)
{
    field(lua, table, key);
    size_t size = 0;
    const char *value = lua_type(lua, -1) == LUA_TSTRING ? lua_tolstring(lua, -1, &size) : NULL;
    bool valid = copy_label(target, capacity, value, size);
    lua_pop(lua, 1);
    return valid;
}

static bool read_arena(lua_State *lua, FodderArena *arena)
{
    field(lua, 1, "arena");
    if (!lua_istable(lua, 2))
        return false;
    bool valid = label(lua, 2, "name", arena->name, sizeof(arena->name));
    arena->half_width = number(lua, 2, "width", 40, 83, &valid);
    arena->half_length = number(lua, 2, "length", 43, 83, &valid);
    arena->channel = number(lua, 2, "channel", 0, 7, &valid);
    arena->ford = number(lua, 2, "ford", 3, 26, &valid);
    arena->dip = number(lua, 2, "dip", 0, 3, &valid);
    field(lua, 2, "obstacles");
    if (!lua_istable(lua, 3) || lua_rawlen(lua, 3) > FODDER_OBSTACLES)
        return false;
    arena->obstacle_count = (unsigned)lua_rawlen(lua, 3);
    for (unsigned i = 0; i < arena->obstacle_count; ++i) {
        lua_rawgeti(lua, 3, i + 1);
        if (!lua_istable(lua, 4))
            return false;
        FodderObstacle *o = &arena->obstacles[i];
        o->x = number(lua, 4, "x", -arena->half_width + 1, arena->half_width - 1, &valid);
        o->z = number(lua, 4, "z", -arena->half_length + 1, arena->half_length - 1, &valid);
        o->half_x = number(lua, 4, "half_x", 0.1f, 12, &valid);
        o->half_z = number(lua, 4, "half_z", 0.1f, 12, &valid);
        o->height = number(lua, 4, "height", 0.3f, 12, &valid);
        lua_pop(lua, 1);
    }
    lua_settop(lua, 1);
    return valid;
}

static bool read_variants(lua_State *lua, FodderPack *pack)
{
    field(lua, 1, "battles");
    if (!lua_istable(lua, 2) || lua_rawlen(lua, 2) == 0 || lua_rawlen(lua, 2) > FODDER_VARIANTS)
        return false;
    pack->variant_count = (unsigned)lua_rawlen(lua, 2);
    bool valid = true;
    for (unsigned i = 0; i < pack->variant_count; ++i) {
        lua_rawgeti(lua, 2, i + 1);
        if (!lua_istable(lua, 3))
            return false;
        FodderVariant *v = &pack->variants[i];
        valid &= label(lua, 3, "name", v->name, sizeof(v->name));
        valid &= label(lua, 3, "hint", v->hint, sizeof(v->hint));
        v->seconds = (unsigned)number(lua, 3, "seconds", 1, 480, &valid);
        field(lua, 3, "flankers");
        if (!lua_isnil(lua, -1)) {
            lua_Integer count = lua_tointeger(lua, -1);
            if (lua_isinteger(lua, -1) && count >= 0 && count <= FODDER_MAX_FLANKERS)
                v->flankers = (unsigned)count;
            else
                valid = false;
        }
        lua_pop(lua, 1);
        v->rout_fraction = number(lua, 3, "rout", 0, 0.95f, &valid);
        v->enemy_speed = number(lua, 3, "enemy_speed", 0, 7, &valid);
        v->ally_speed = number(lua, 3, "ally_speed", 0, 7, &valid);
        v->enemy_damage = number(lua, 3, "enemy_damage", 0, 150, &valid);
        v->ally_damage = number(lua, 3, "ally_damage", 0, 150, &valid);
        v->player_damage = number(lua, 3, "player_damage", 1, 150, &valid);
        v->fog = number(lua, 3, "fog", 12, 250, &valid);
        v->ally_lane = (int)number(lua, 3, "ally_lane", -1, 1, &valid);
        v->enemy_lane = (int)number(lua, 3, "enemy_lane", -1, 1, &valid);
        lua_pop(lua, 1);
    }
    lua_settop(lua, 1);
    return valid;
}

static bool read_upgrades(lua_State *lua, FodderPack *pack)
{
    field(lua, 1, "upgrades");
    if (!lua_istable(lua, 2) || lua_rawlen(lua, 2) != FODDER_UPGRADES)
        return false;
    bool valid = true;
    for (unsigned i = 0; i < FODDER_UPGRADES; ++i) {
        lua_rawgeti(lua, 2, i + 1);
        if (!lua_istable(lua, 3))
            return false;
        FodderUpgrade *upgrade = &pack->upgrades[i];
        valid &= label(lua, 3, "name", upgrade->name, sizeof(upgrade->name));
        valid &= label(lua, 3, "description", upgrade->description, sizeof(upgrade->description));
        lua_pop(lua, 1);
    }
    lua_settop(lua, 1);
    field(lua, 1, "effects");
    return valid && lua_isfunction(lua, -1);
}

static bool read_effects(const FodderPack *pack, const unsigned ranks[FODDER_UPGRADES],
                         FodderEffects *effects)
{
    lua_State *lua = pack->script;
    lua_rawgeti(lua, LUA_REGISTRYINDEX, pack->rules_reference);
    field(lua, -1, "effects");
    for (unsigned i = 0; i < FODDER_UPGRADES; ++i)
        lua_pushinteger(lua, ranks[i]);
    bool valid = run_script(lua, FODDER_UPGRADES, 1) == LUA_OK && lua_istable(lua, -1);
    if (valid) {
        effects->recovery = number(lua, -1, "recovery", 0.5f, 2.0f, &valid);
        effects->block_cost = number(lua, -1, "block_cost", 0.35f, 2.0f, &valid);
        effects->dressing = number(lua, -1, "dressing", 0, 45, &valid);
    }
    lua_settop(lua, 0);
    return valid;
}

bool fodder_pack_open(FodderPack *pack, const char *root, char error[FODDER_ERROR])
{
    memset(pack, 0, sizeof(*pack));
    if (strlen(root) >= sizeof(pack->root)) {
        snprintf(error, FODDER_ERROR, "Pack folder path is too long.");
        return false;
    }
    memcpy(pack->root, root, strlen(root) + 1);
    if (!read_manifest(pack)) {
        snprintf(error, FODDER_ERROR, "Invalid pack.json. Check API version, ID, title and file paths.");
        return false;
    }
    char path[FODDER_PATH];
    size_t length;
    if (!fodder_pack_file(pack, FODDER_SCRIPT, path)) {
        snprintf(error, FODDER_ERROR, "Lua file path is too long.");
        return false;
    }
    char *source = fodder_read(path, 256 * 1024, &length);
    lua_State *lua = source ? new_script() : NULL;
    if (!lua) {
        free(source);
        snprintf(error, FODDER_ERROR, "Could not open the pack's Lua script.");
        return false;
    }
    pack->script = lua;
    int status = luaL_loadbufferx(lua, source, length, pack->files[FODDER_SCRIPT], "t");
    free(source);
    if (status == LUA_OK)
        status = run_script(lua, 0, 1);
    if (status != LUA_OK) {
        const char *message = lua_tostring(lua, -1);
        snprintf(error, FODDER_ERROR, "Lua: %s", message ? message : "script error");
    } else if (lua_istable(lua, 1) && read_arena(lua, &pack->arena) &&
               read_variants(lua, pack) && read_upgrades(lua, pack)) {
        lua_settop(lua, 1);
        pack->rules_reference = luaL_ref(lua, LUA_REGISTRYINDEX);
        const unsigned ranks[FODDER_UPGRADES] = {0};
        FodderEffects effects;
        if (read_effects(pack, ranks, &effects)) {
            error[0] = '\0';
            return true;
        }
        snprintf(error, FODDER_ERROR, "Lua upgrade effects must return valid numbers.");
    } else {
        snprintf(error, FODDER_ERROR, "Invalid Lua arena, battles or upgrades.");
    }
    fodder_pack_close(pack);
    return false;
}

void fodder_pack_close(FodderPack *pack)
{
    if (!pack->script)
        return;
    lua_State *lua = pack->script;
    void *data;
    lua_getallocf(lua, &data);
    lua_close(lua);
    free(data);
    pack->script = NULL;
}

FodderEffects fodder_pack_effects(const FodderPack *pack, const unsigned ranks[FODDER_UPGRADES])
{
    FodderEffects effects;
    if (read_effects(pack, ranks, &effects))
        return effects;
    return (FodderEffects){.recovery = 1.0f, .block_cost = 1.0f};
}
