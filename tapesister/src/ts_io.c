#include "tapesister/ts_io.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define TS_UNLINK _unlink
#define TS_GETPID _getpid
#else
#include <unistd.h>
#define TS_UNLINK unlink
#define TS_GETPID getpid
#endif

enum { FIELD_COUNT = 36 };
static const char *const field_names[FIELD_COUNT] =
{
    "schema", "schema_version", "renderer_version", "name", "seed",
    "sample_rate", "requested_frames", "root_midi_note", "fine_tune_cent100",
    "source", "source_shape_ppm", "harmonic_mix_ppm", "noise_type",
    "noise_amount_ppm", "attack_us", "decay_us", "sustain_ppm", "release_us",
    "pitch_env_cent100", "pitch_env_us", "filter_enabled", "filter_mode",
    "filter_cutoff_millihz", "filter_resonance_ppm", "filter_env_octave_cent100",
    "shaper", "drive_ppm", "delay_us", "delay_feedback_ppm", "delay_mix_ppm",
    "reverb_decay_ppm", "reverb_mix_ppm", "finishing_mode", "target_peak_ppm",
    "fixed_gain_centidb", "format_end"
};

typedef struct parser
{
    const char *text;
    size_t length, at;
    ts_io_error *error;
} parser;

static void set_error(ts_io_error *e, ts_io_status status, size_t offset,
    const char *message)
{
    if (e == NULL) return;
    e->status = status;
    e->offset = offset;
    snprintf(e->message, sizeof(e->message), "%s", message);
}

static bool publish_no_replace(const char *temporary, const char *destination)
{
#ifdef _WIN32
    return MoveFileA(temporary, destination) != 0;
#else
    if (link(temporary, destination) != 0) return false;
    if (unlink(temporary) != 0) { unlink(destination); return false; }
    return true;
#endif
}

static void skip_ws(parser *p)
{
    while (p->at < p->length && (p->text[p->at] == ' ' ||
        p->text[p->at] == '\t' || p->text[p->at] == '\r' ||
        p->text[p->at] == '\n')) p->at++;
}

static bool valid_utf8(const char *s, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        const uint8_t c = (uint8_t)s[i];
        if (c < 0x80) continue;
        size_t extra; uint32_t code;
        if ((c & 0xe0) == 0xc0) { extra = 1; code = c & 0x1f; if (code < 2) return false; }
        else if ((c & 0xf0) == 0xe0) { extra = 2; code = c & 0x0f; }
        else if ((c & 0xf8) == 0xf0) { extra = 3; code = c & 0x07; }
        else return false;
        if (i + extra >= length) return false;
        for (size_t j = 0; j < extra; j++)
        {
            const uint8_t next = (uint8_t)s[++i];
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 0x3f);
        }
        if ((extra == 2 && code < 0x800) || (extra == 3 && code < 0x10000) ||
            code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}

static bool take(parser *p, char c)
{
    skip_ws(p);
    if (p->at >= p->length || p->text[p->at] != c) return false;
    p->at++;
    return true;
}

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parse_hex4(parser *p, uint32_t *value)
{
    if (p->length - p->at < 4) return false;
    uint32_t result = 0;
    for (int i = 0; i < 4; i++)
    {
        const int digit = hex_digit((unsigned char)p->text[p->at++]);
        if (digit < 0) return false;
        result = (result << 4) | (uint32_t)digit;
    }
    *value = result; return true;
}

static bool append_utf8(char *out, size_t capacity, size_t *used, uint32_t code)
{
    size_t count;
    if (code <= 0x7f) count = 1;
    else if (code <= 0x7ff) count = 2;
    else if (code <= 0xffff) count = 3;
    else count = 4;
    if (*used + count >= capacity) return false;
    if (count == 1) out[(*used)++] = (char)code;
    else if (count == 2)
    {
        out[(*used)++] = (char)(0xc0 | (code >> 6));
        out[(*used)++] = (char)(0x80 | (code & 0x3f));
    }
    else if (count == 3)
    {
        out[(*used)++] = (char)(0xe0 | (code >> 12));
        out[(*used)++] = (char)(0x80 | ((code >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (code & 0x3f));
    }
    else
    {
        out[(*used)++] = (char)(0xf0 | (code >> 18));
        out[(*used)++] = (char)(0x80 | ((code >> 12) & 0x3f));
        out[(*used)++] = (char)(0x80 | ((code >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (code & 0x3f));
    }
    return true;
}

static bool parse_string(parser *p, char *out, size_t capacity, bool *tooLong)
{
    if (tooLong != NULL) *tooLong = false;
    skip_ws(p);
    if (p->at >= p->length || p->text[p->at++] != '"') return false;
    size_t used = 0;
    while (p->at < p->length)
    {
        unsigned char c = (unsigned char)p->text[p->at++];
        if (c == '"')
        {
            if (used >= capacity)
            {
                if (tooLong != NULL) *tooLong = true;
                return false;
            }
            out[used] = '\0';
            return true;
        }
        if (c < 0x20 || c == 0x7f) return false;
        if (c == '\\')
        {
            if (p->at >= p->length) return false;
            c = (unsigned char)p->text[p->at++];
            if (c == '"' || c == '\\' || c == '/') { }
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c == 'u')
            {
                uint32_t code;
                if (!parse_hex4(p, &code)) return false;
                if (code >= 0xd800 && code <= 0xdbff)
                {
                    if (p->length - p->at < 6 || p->text[p->at++] != '\\' ||
                        p->text[p->at++] != 'u') return false;
                    uint32_t low;
                    if (!parse_hex4(p, &low) || low < 0xdc00 || low > 0xdfff) return false;
                    code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                }
                else if (code >= 0xdc00 && code <= 0xdfff) return false;
                if (!append_utf8(out, capacity, &used, code))
                {
                    if (tooLong != NULL) *tooLong = true;
                    return false;
                }
                continue;
            }
            else return false;
        }
        if (used + 1 >= capacity)
        {
            if (tooLong != NULL) *tooLong = true;
            return false;
        }
        out[used++] = (char)c;
    }
    return false;
}

static bool parse_i64(parser *p, int64_t *value)
{
    skip_ws(p);
    const size_t start = p->at;
    if (p->at < p->length && p->text[p->at] == '-') p->at++;
    if (p->at >= p->length || !isdigit((unsigned char)p->text[p->at])) return false;
    if (p->text[p->at] == '0' && p->at + 1 < p->length &&
        isdigit((unsigned char)p->text[p->at + 1])) return false;
    while (p->at < p->length && isdigit((unsigned char)p->text[p->at])) p->at++;
    if (p->at - start >= 32) return false;
    char number[32];
    memcpy(number, p->text + start, p->at - start);
    number[p->at - start] = '\0';
    errno = 0;
    char *end = NULL;
    const long long parsed = strtoll(number, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0') return false;
    *value = (int64_t)parsed;
    return true;
}

static bool parse_bool(parser *p, bool *value)
{
    skip_ws(p);
    if (p->length - p->at >= 4 && memcmp(p->text + p->at, "true", 4) == 0)
    { p->at += 4; *value = true; return true; }
    if (p->length - p->at >= 5 && memcmp(p->text + p->at, "false", 5) == 0)
    { p->at += 5; *value = false; return true; }
    return false;
}

static int field_index(const char *name)
{
    for (int i = 0; i < FIELD_COUNT; i++)
        if (strcmp(name, field_names[i]) == 0) return i;
    return -1;
}

static bool assign_string(ts_recipe *r, int field, const char *value,
    char *name_storage, uint64_t *seed, int64_t *schema, int64_t *renderer)
{
    (void)schema; (void)renderer;
    if (field == 0) return strcmp(value, "tapesister.recipe") == 0;
    if (field == 3)
    {
        const size_t length = strlen(value);
        if (length == 0 || length > TS_RECIPE_NAME_MAX_BYTES) return false;
        memcpy(name_storage, value, length + 1);
        r->name = name_storage;
        return true;
    }
    if (field == 4)
    {
        if (strlen(value) != 16) return false;
        uint64_t n = 0;
        for (size_t i = 0; i < 16; i++)
        {
            const char c = value[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
            n = (n << 4) | (uint64_t)(c <= '9' ? c - '0' : c - 'a' + 10);
        }
        *seed = n; r->seed = n; return true;
    }
    if (field == 9)
    {
        const char *names[] = { "sine", "triangle", "saw", "pulse", "click" };
        for (int i = 0; i < 5; i++) if (strcmp(value, names[i]) == 0)
        { r->source = (ts_source_type)i; return true; }
    }
    if (field == 12)
    {
        const char *names[] = { "white", "pinkish", "metallic" };
        for (int i = 0; i < 3; i++) if (strcmp(value, names[i]) == 0)
        { r->noise_type = (ts_noise_type)i; return true; }
    }
    if (field == 21)
    {
        const char *names[] = { "low_pass", "band_pass", "high_pass", "notch" };
        for (int i = 0; i < 4; i++) if (strcmp(value, names[i]) == 0)
        { r->filter_mode = (ts_filter_mode)i; return true; }
    }
    if (field == 25)
    {
        const char *names[] = { "soft", "hard", "fold" };
        for (int i = 0; i < 3; i++) if (strcmp(value, names[i]) == 0)
        { r->shaper = (ts_shaper_type)i; return true; }
    }
    if (field == 32)
    {
        if (strcmp(value, "target_peak") == 0) { r->finishing_mode = TS_FINISH_TARGET_PEAK; return true; }
        if (strcmp(value, "fixed_headroom") == 0) { r->finishing_mode = TS_FINISH_FIXED_HEADROOM; return true; }
    }
    return false;
}

static bool assign_integer(ts_recipe *r, int f, int64_t v,
    int64_t *schema, int64_t *renderer)
{
    if (f == 1) { *schema = v; return true; }
    if (f == 2) { *renderer = v; return true; }
    if (f == 5 && v >= 8000 && v <= 192000) r->sample_rate = (uint32_t)v;
    else if (f == 6 && v >= 160 && v <= 1920000) r->requested_frames = (uint32_t)v;
    else if (f == 7 && v >= 0 && v <= 127) r->root_midi_note = (uint8_t)v;
    else if (f == 8 && v >= -10000 && v <= 10000) r->fine_tune_cent100 = (int32_t)v;
    else if (f == 10 && v >= 50000 && v <= 950000) r->source_shape = (float)v / 1000000.0f;
    else if (f == 11 && v >= 0 && v <= 1000000) r->harmonic_mix = (float)v / 1000000.0f;
    else if (f == 13 && v >= 0 && v <= 1000000) r->noise_amount = (float)v / 1000000.0f;
    else if (f == 14 && v >= 0 && v <= 10000000) r->attack_seconds = (float)v / 1000000.0f;
    else if (f == 15 && v > 0 && v <= 10000000) r->decay_seconds = (float)v / 1000000.0f;
    else if (f == 16 && v >= 0 && v <= 1000000) r->sustain_level = (float)v / 1000000.0f;
    else if (f == 17 && v >= 0 && v <= 10000000) r->release_seconds = (float)v / 1000000.0f;
    else if (f == 18 && v >= -960000 && v <= 960000) r->pitch_env_semitones = (float)v / 10000.0f;
    else if (f == 19 && v > 0 && v <= 10000000) r->pitch_env_seconds = (float)v / 1000000.0f;
    else if (f == 22 && v >= 20000 && v <= 86400000) r->filter_cutoff_hz = (float)v / 1000.0f;
    else if (f == 23 && v >= 0 && v <= 950000) r->filter_resonance = (float)v / 1000000.0f;
    else if (f == 24 && v >= -800 && v <= 800) r->filter_env_octaves = (float)v / 100.0f;
    else if (f == 26 && v >= 100000 && v <= 8000000) r->drive = (float)v / 1000000.0f;
    else if (f == 27 && v >= 0 && v <= 1000000) r->delay_seconds = (float)v / 1000000.0f;
    else if (f == 28 && v >= 0 && v <= 850000) r->delay_feedback = (float)v / 1000000.0f;
    else if (f == 29 && v >= 0 && v <= 1000000) r->delay_mix = (float)v / 1000000.0f;
    else if (f == 30 && v >= 0 && v <= 900000) r->reverb_decay = (float)v / 1000000.0f;
    else if (f == 31 && v >= 0 && v <= 1000000) r->reverb_mix = (float)v / 1000000.0f;
    else if (f == 33 && v >= 100000 && v <= 950000) r->target_peak = (float)v / 1000000.0f;
    else if (f == 34 && v >= -9600 && v <= 0) r->fixed_gain_centidb = (int32_t)v;
    else return f == 1 || f == 2;
    return true;
}

ts_io_status ts_recipe_parse(const char *json, const size_t length,
    ts_recipe *recipe, ts_io_error *error)
{
    if (json == NULL || recipe == NULL || length == 0)
    { set_error(error, TS_IO_INVALID_ARGUMENT, 0, "empty recipe input"); return TS_IO_INVALID_ARGUMENT; }
    if (length > TS_RECIPE_MAX_BYTES)
    { set_error(error, TS_IO_TOO_LARGE, 0, "recipe exceeds 65536 bytes"); return TS_IO_TOO_LARGE; }
    if (!valid_utf8(json, length))
    { set_error(error, TS_IO_PARSE_ERROR, 0, "recipe is not valid UTF-8"); return TS_IO_PARSE_ERROR; }
    parser p = { json, length, 0, error };
    ts_recipe r = { 0 };
    char name[128] = { 0 }, key[64], string_value[160];
    uint64_t seen = 0, seed = 0;
    int64_t schema = -1, renderer = -1;
    if (!take(&p, '{')) goto malformed;
    for (int item = 0; ; item++)
    {
        skip_ws(&p);
        if (p.at < p.length && p.text[p.at] == '}') { p.at++; break; }
        if (item >= FIELD_COUNT || (item > 0 && !take(&p, ',')) ||
            !parse_string(&p, key, sizeof(key), NULL) || !take(&p, ':')) goto malformed;
        const int f = field_index(key);
        if (f < 0 || f == 35)
        { set_error(error, TS_IO_UNKNOWN_FIELD, p.at, "unknown recipe field"); return TS_IO_UNKNOWN_FIELD; }
        if ((seen & (UINT64_C(1) << f)) != 0)
        { set_error(error, TS_IO_DUPLICATE_FIELD, p.at, "duplicate recipe field"); return TS_IO_DUPLICATE_FIELD; }
        seen |= UINT64_C(1) << f;
        bool ok;
        if (f == 0 || f == 3 || f == 4 || f == 9 || f == 12 || f == 21 || f == 25 || f == 32)
        {
            bool stringTooLong = false;
            ok = parse_string(&p, string_value, sizeof(string_value), &stringTooLong);
            if (f == 3 && (stringTooLong ||
                (ok && strlen(string_value) > TS_RECIPE_NAME_MAX_BYTES)))
            {
                set_error(error, TS_IO_INVALID_VALUE, p.at,
                    "recipe name exceeds 127 decoded UTF-8 bytes");
                return TS_IO_INVALID_VALUE;
            }
            ok = ok && assign_string(&r, f, string_value, name, &seed,
                &schema, &renderer);
        }
        else if (f == 20)
            ok = parse_bool(&p, &r.filter_enabled);
        else
        {
            int64_t v;
            ok = parse_i64(&p, &v) && assign_integer(&r, f, v, &schema, &renderer);
        }
        if (!ok)
        { set_error(error, TS_IO_INVALID_VALUE, p.at, "invalid recipe field value"); return TS_IO_INVALID_VALUE; }
    }
    skip_ws(&p);
    if (p.at != p.length) goto malformed;
    if (seen != ((UINT64_C(1) << 35) - 1))
    { set_error(error, TS_IO_MISSING_FIELD, p.at, "missing required recipe field"); return TS_IO_MISSING_FIELD; }
    if (schema != 1 || renderer != TS_RENDERER_VERSION)
    { set_error(error, TS_IO_UNSUPPORTED_VERSION, 0, "unsupported schema or renderer version"); return TS_IO_UNSUPPORTED_VERSION; }
    if (!ts_recipe_validate(&r))
    { set_error(error, TS_IO_INVALID_VALUE, 0, "recipe values fail renderer validation"); return TS_IO_INVALID_VALUE; }
    /* Names loaded through this API are owned by stable storage in the result.
     * The public recipe retains a pointer, so copy into a per-call heap block. */
    const size_t name_length = strlen(name);
    char *owned_name = malloc(name_length + 1);
    if (owned_name == NULL)
    { set_error(error, TS_IO_OPEN_FAILED, 0, "out of memory"); return TS_IO_OPEN_FAILED; }
    memcpy(owned_name, name, name_length + 1);
    r.name = owned_name;
    *recipe = r;
    set_error(error, TS_IO_OK, 0, "ok");
    return TS_IO_OK;
malformed:
    set_error(error, TS_IO_PARSE_ERROR, p.at, "malformed or excessively nested JSON");
    return TS_IO_PARSE_ERROR;
}

static const char *source_name(ts_source_type v) { static const char *n[] = {"sine","triangle","saw","pulse","click"}; return n[v]; }
static const char *noise_name(ts_noise_type v) { static const char *n[] = {"white","pinkish","metallic"}; return n[v]; }
static const char *filter_name(ts_filter_mode v) { static const char *n[] = {"low_pass","band_pass","high_pass","notch"}; return n[v]; }
static const char *shaper_name(ts_shaper_type v) { static const char *n[] = {"soft","hard","fold"}; return n[v]; }

static long long million(float v) { return llround((double)v * 1000000.0); }
static long long micros(float v) { return llround((double)v * 1000000.0); }

static bool append_escaped(char *dst, size_t capacity, size_t *used, const char *s)
{
    for (; *s != '\0'; s++)
    {
        char escape = 0;
        if (*s == '"' || *s == '\\') escape = *s;
        else if (*s == '\n') escape = 'n';
        else if (*s == '\r') escape = 'r';
        else if (*s == '\t') escape = 't';
        else if ((unsigned char)*s < 0x20) return false;
        if (escape != 0)
        {
            if (*used + 2 >= capacity) return false;
            dst[(*used)++] = '\\'; dst[(*used)++] = escape;
        }
        else { if (*used + 1 >= capacity) return false; dst[(*used)++] = *s; }
    }
    dst[*used] = '\0'; return true;
}

ts_io_status ts_recipe_format(const ts_recipe *r, char **json, size_t *length,
    ts_io_error *error)
{
    if (!ts_recipe_validate(r) || strlen(r->name) > TS_RECIPE_NAME_MAX_BYTES ||
        json == NULL || length == NULL)
    { set_error(error, TS_IO_INVALID_ARGUMENT, 0, "invalid recipe for formatting"); return TS_IO_INVALID_ARGUMENT; }
    char escaped[512] = { 0 }; size_t escaped_len = 0;
    if (!append_escaped(escaped, sizeof(escaped), &escaped_len, r->name))
    { set_error(error, TS_IO_INVALID_VALUE, 0, "recipe name cannot be encoded"); return TS_IO_INVALID_VALUE; }
    char *out = malloc(8192);
    if (out == NULL) return TS_IO_OPEN_FAILED;
    const int n = snprintf(out, 8192,
        "{\n"
        "  \"schema\": \"tapesister.recipe\",\n"
        "  \"schema_version\": 1,\n"
        "  \"renderer_version\": 1,\n"
        "  \"name\": \"%s\",\n"
        "  \"seed\": \"%016llx\",\n"
        "  \"sample_rate\": %u,\n"
        "  \"requested_frames\": %u,\n"
        "  \"root_midi_note\": %u,\n"
        "  \"fine_tune_cent100\": %d,\n"
        "  \"source\": \"%s\",\n"
        "  \"source_shape_ppm\": %lld,\n"
        "  \"harmonic_mix_ppm\": %lld,\n"
        "  \"noise_type\": \"%s\",\n"
        "  \"noise_amount_ppm\": %lld,\n"
        "  \"attack_us\": %lld,\n"
        "  \"decay_us\": %lld,\n"
        "  \"sustain_ppm\": %lld,\n"
        "  \"release_us\": %lld,\n"
        "  \"pitch_env_cent100\": %lld,\n"
        "  \"pitch_env_us\": %lld,\n"
        "  \"filter_enabled\": %s,\n"
        "  \"filter_mode\": \"%s\",\n"
        "  \"filter_cutoff_millihz\": %lld,\n"
        "  \"filter_resonance_ppm\": %lld,\n"
        "  \"filter_env_octave_cent100\": %lld,\n"
        "  \"shaper\": \"%s\",\n"
        "  \"drive_ppm\": %lld,\n"
        "  \"delay_us\": %lld,\n"
        "  \"delay_feedback_ppm\": %lld,\n"
        "  \"delay_mix_ppm\": %lld,\n"
        "  \"reverb_decay_ppm\": %lld,\n"
        "  \"reverb_mix_ppm\": %lld,\n"
        "  \"finishing_mode\": \"%s\",\n"
        "  \"target_peak_ppm\": %lld,\n"
        "  \"fixed_gain_centidb\": %d\n"
        "}\n", escaped, (unsigned long long)r->seed, r->sample_rate,
        r->requested_frames, r->root_midi_note, r->fine_tune_cent100,
        source_name(r->source), million(r->source_shape), million(r->harmonic_mix),
        noise_name(r->noise_type), million(r->noise_amount), micros(r->attack_seconds),
        micros(r->decay_seconds), million(r->sustain_level), micros(r->release_seconds),
        (long long)llround((double)r->pitch_env_semitones * 10000.0),
        micros(r->pitch_env_seconds), r->filter_enabled ? "true" : "false",
        filter_name(r->filter_mode), (long long)llround((double)r->filter_cutoff_hz * 1000.0),
        million(r->filter_resonance), (long long)llround((double)r->filter_env_octaves * 100.0),
        shaper_name(r->shaper), million(r->drive), micros(r->delay_seconds),
        million(r->delay_feedback), million(r->delay_mix), million(r->reverb_decay),
        million(r->reverb_mix), r->finishing_mode == TS_FINISH_TARGET_PEAK ?
        "target_peak" : "fixed_headroom", million(r->target_peak), r->fixed_gain_centidb);
    if (n < 0 || n >= 8192) { free(out); return TS_IO_TOO_LARGE; }
    *json = out; *length = (size_t)n; set_error(error, TS_IO_OK, 0, "ok"); return TS_IO_OK;
}

ts_io_status ts_recipe_load_file(const char *path, ts_recipe *r, ts_io_error *e)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { set_error(e, TS_IO_OPEN_FAILED, 0, "cannot open recipe"); return TS_IO_OPEN_FAILED; }
    char *data = malloc(TS_RECIPE_MAX_BYTES + 1U); size_t n = 0;
    if (data != NULL) n = fread(data, 1, TS_RECIPE_MAX_BYTES + 1U, f);
    const bool read_error = ferror(f) != 0; fclose(f);
    if (data == NULL || read_error) { free(data); return TS_IO_OPEN_FAILED; }
    if (n > TS_RECIPE_MAX_BYTES) { free(data); set_error(e, TS_IO_TOO_LARGE, 0, "recipe too large"); return TS_IO_TOO_LARGE; }
    const ts_io_status status = ts_recipe_parse(data, n, r, e); free(data); return status;
}

void ts_recipe_loaded_dispose(ts_recipe *recipe)
{
    if (recipe == NULL) return;
    free((void *)recipe->name);
    memset(recipe, 0, sizeof(*recipe));
}

static ts_io_status atomic_write(const char *path, const uint8_t *data,
    size_t length, size_t fail_after, ts_io_error *e)
{
    if (path == NULL || data == NULL) return TS_IO_INVALID_ARGUMENT;
    FILE *existing = fopen(path, "rb");
    if (existing != NULL) { fclose(existing); set_error(e, TS_IO_EXISTS, 0, "destination exists"); return TS_IO_EXISTS; }
    char temp[1024]; FILE *f = NULL;
    for (unsigned int attempt = 0; attempt < 64U; attempt++)
    {
        const int count = snprintf(temp, sizeof(temp), "%s.tmp.%ld.%u", path,
            (long)TS_GETPID(), attempt);
        if (count < 0 || (size_t)count >= sizeof(temp)) return TS_IO_INVALID_ARGUMENT;
        f = fopen(temp, "wbx");
        if (f != NULL) break;
    }
    if (f == NULL) { set_error(e, TS_IO_OPEN_FAILED, 0, "cannot create temporary file"); return TS_IO_OPEN_FAILED; }
    size_t requested = length;
    if (fail_after != TS_IO_NO_FAILURE && fail_after < requested) requested = fail_after;
    bool failed = fwrite(data, 1, requested, f) != requested || requested != length;
    if (fflush(f) != 0) failed = true;
    if (fclose(f) != 0) failed = true;
    if (failed) { TS_UNLINK(temp); set_error(e, TS_IO_WRITE_FAILED, requested, "temporary write failed"); return TS_IO_WRITE_FAILED; }
    if (!publish_no_replace(temp, path)) { TS_UNLINK(temp); set_error(e, TS_IO_RENAME_FAILED, 0, "publish failed without replacement"); return TS_IO_RENAME_FAILED; }
    set_error(e, TS_IO_OK, 0, "ok"); return TS_IO_OK;
}

ts_io_status ts_recipe_save_file_test(const char *path, const ts_recipe *r,
    size_t fail_after, ts_io_error *e)
{
    char *json = NULL; size_t length = 0;
    ts_io_status s = ts_recipe_format(r, &json, &length, e);
    if (s == TS_IO_OK) s = atomic_write(path, (const uint8_t *)json, length, fail_after, e);
    free(json); return s;
}

ts_io_status ts_recipe_save_file(const char *path, const ts_recipe *r, ts_io_error *e)
{ return ts_recipe_save_file_test(path, r, TS_IO_NO_FAILURE, e); }

bool ts_pcm16_encode_sample(float x, int16_t *encoded)
{
    if (encoded == NULL || !isfinite(x)) return false;
    if (x > 1.0f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
    const double scaled = (double)x * 32768.0;
    int32_t value = scaled >= 0.0 ? (int32_t)floor(scaled + 0.5) : (int32_t)ceil(scaled - 0.5);
    if (value > 32767) value = 32767; else if (value < -32768) value = -32768;
    *encoded = (int16_t)value; return true;
}

ts_io_status ts_pcm16_encode(const ts_rendered_sample *s, uint8_t **bytes,
    size_t *length, ts_io_error *e)
{
    if (s == NULL || s->samples == NULL || bytes == NULL || length == NULL ||
        s->frame_count > (SIZE_MAX / 2U)) return TS_IO_INVALID_ARGUMENT;
    *length = s->frame_count * 2U; *bytes = malloc(*length);
    if (*bytes == NULL) return TS_IO_OPEN_FAILED;
    for (size_t i = 0; i < s->frame_count; i++)
    {
        int16_t value;
        if (!ts_pcm16_encode_sample(s->samples[i], &value))
        { free(*bytes); *bytes = NULL; set_error(e, TS_IO_INVALID_VALUE, i, "non-finite PCM input"); return TS_IO_INVALID_VALUE; }
        const uint16_t bits = (uint16_t)value;
        (*bytes)[i * 2U] = (uint8_t)bits;
        (*bytes)[i * 2U + 1U] = (uint8_t)(bits >> 8);
    }
    return TS_IO_OK;
}

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }

ts_io_status ts_wav_save_file_test(const char *path, const ts_rendered_sample *s,
    size_t fail_after, ts_io_error *e)
{
    uint8_t *pcm = NULL; size_t pcm_len = 0;
    ts_io_status status = ts_pcm16_encode(s, &pcm, &pcm_len, e);
    if (status != TS_IO_OK) return status;
    if (pcm_len > UINT32_MAX - 36U || pcm_len > SIZE_MAX - 44U)
    { free(pcm); return TS_IO_TOO_LARGE; }
    const size_t total = pcm_len + 44U; uint8_t *wav = calloc(total, 1);
    if (wav == NULL) { free(pcm); return TS_IO_OPEN_FAILED; }
    memcpy(wav, "RIFF", 4); put_u32(wav + 4, (uint32_t)(total - 8U));
    memcpy(wav + 8, "WAVEfmt ", 8); put_u32(wav + 16, 16); put_u16(wav + 20, 1);
    put_u16(wav + 22, 1); put_u32(wav + 24, s->sample_rate);
    if (s->sample_rate > UINT32_MAX / 2U) { free(wav); free(pcm); return TS_IO_TOO_LARGE; }
    put_u32(wav + 28, s->sample_rate * 2U); put_u16(wav + 32, 2); put_u16(wav + 34, 16);
    memcpy(wav + 36, "data", 4); put_u32(wav + 40, (uint32_t)pcm_len);
    memcpy(wav + 44, pcm, pcm_len); free(pcm);
    status = atomic_write(path, wav, total, fail_after, e); free(wav); return status;
}

ts_io_status ts_wav_save_file(const char *path, const ts_rendered_sample *s, ts_io_error *e)
{ return ts_wav_save_file_test(path, s, TS_IO_NO_FAILURE, e); }

static bool path_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;
    fclose(f); return true;
}

ts_io_status ts_bake_pair_files_test(const char *recipe_path,
    const char *wav_path, const ts_recipe *recipe,
    const ts_rendered_sample *sample, const size_t recipe_fail_after,
    const size_t wav_fail_after, ts_io_error *error)
{
    if (recipe_path == NULL || wav_path == NULL || strcmp(recipe_path, wav_path) == 0)
        return TS_IO_INVALID_ARGUMENT;
    if (path_exists(recipe_path) || path_exists(wav_path))
    { set_error(error, TS_IO_EXISTS, 0, "recipe or WAV destination exists"); return TS_IO_EXISTS; }
    char recipe_stage[1024], wav_stage[1024];
    bool stage_found = false;
    for (unsigned int attempt = 0; attempt < 64U; attempt++)
    {
        const int recipe_count = snprintf(recipe_stage, sizeof(recipe_stage),
            "%s.pair.%ld.%u", recipe_path, (long)TS_GETPID(), attempt);
        const int wav_count = snprintf(wav_stage, sizeof(wav_stage),
            "%s.pair.%ld.%u", wav_path, (long)TS_GETPID(), attempt);
        if (recipe_count < 0 || (size_t)recipe_count >= sizeof(recipe_stage) ||
            wav_count < 0 || (size_t)wav_count >= sizeof(wav_stage))
            return TS_IO_INVALID_ARGUMENT;
        if (!path_exists(recipe_stage) && !path_exists(wav_stage))
        { stage_found = true; break; }
    }
    if (!stage_found) return TS_IO_OPEN_FAILED;
    ts_io_status status = ts_recipe_save_file_test(recipe_stage, recipe,
        recipe_fail_after, error);
    if (status == TS_IO_OK)
        status = ts_wav_save_file_test(wav_stage, sample, wav_fail_after, error);
    if (status != TS_IO_OK)
    { TS_UNLINK(recipe_stage); TS_UNLINK(wav_stage); return status; }
    if (path_exists(recipe_path) || path_exists(wav_path))
    { TS_UNLINK(recipe_stage); TS_UNLINK(wav_stage); set_error(error, TS_IO_EXISTS, 0, "destination appeared during publish"); return TS_IO_EXISTS; }
    if (!publish_no_replace(recipe_stage, recipe_path))
    { TS_UNLINK(recipe_stage); TS_UNLINK(wav_stage); return TS_IO_RENAME_FAILED; }
    if (!publish_no_replace(wav_stage, wav_path))
    {
        TS_UNLINK(recipe_path); TS_UNLINK(wav_stage);
        set_error(error, TS_IO_RENAME_FAILED, 0, "pair publish rolled back");
        return TS_IO_RENAME_FAILED;
    }
    set_error(error, TS_IO_OK, 0, "ok"); return TS_IO_OK;
}

ts_io_status ts_bake_pair_files(const char *recipe_path, const char *wav_path,
    const ts_recipe *recipe, const ts_rendered_sample *sample,
    ts_io_error *error)
{
    return ts_bake_pair_files_test(recipe_path, wav_path, recipe, sample,
        TS_IO_NO_FAILURE, TS_IO_NO_FAILURE, error);
}

static bool unique_sibling(const char *path, const char *tag, char *out,
    size_t capacity)
{
    for (unsigned int attempt = 0; attempt < 64U; attempt++)
    {
        const int count = snprintf(out, capacity, "%s.%s.%ld.%u", path, tag,
            (long)TS_GETPID(), attempt);
        if (count < 0 || (size_t)count >= capacity) return false;
        if (!path_exists(out)) return true;
    }
    return false;
}

ts_io_status ts_recipe_replace_file_test(const char *path,
    const ts_recipe *recipe, const bool fail_publish, const bool fail_restore,
    ts_io_error *error)
{
    if (path == NULL || recipe == NULL) return TS_IO_INVALID_ARGUMENT;
    if (!path_exists(path)) return ts_recipe_save_file(path, recipe, error);
    char stage[1024], backup[1024];
    if (!unique_sibling(path, "replace", stage, sizeof stage) ||
        !unique_sibling(path, "backup", backup, sizeof backup))
        return TS_IO_OPEN_FAILED;
    ts_io_status status = ts_recipe_save_file(stage, recipe, error);
    if (status != TS_IO_OK) return status;
    if (rename(path, backup) != 0)
    { TS_UNLINK(stage); return TS_IO_RENAME_FAILED; }
    if (fail_publish || !publish_no_replace(stage, path))
    {
        const bool restored = !fail_restore && rename(backup, path) == 0;
        TS_UNLINK(stage);
        if (!restored)
        {
            char message[160];
            snprintf(message, sizeof message,
                "recovery incomplete: %.120s retained", backup);
            set_error(error, TS_IO_ROLLBACK_FAILED, 0, message);
            return TS_IO_ROLLBACK_FAILED;
        }
        set_error(error, TS_IO_RENAME_FAILED, 0, "replacement rolled back");
        return TS_IO_RENAME_FAILED;
    }
    TS_UNLINK(backup); set_error(error, TS_IO_OK, 0, "ok"); return TS_IO_OK;
}
ts_io_status ts_recipe_replace_file(const char *path, const ts_recipe *recipe,
    ts_io_error *error)
{ return ts_recipe_replace_file_test(path, recipe, false, false, error); }

ts_io_status ts_bake_pair_replace_files_test(const char *recipe_path,
    const char *wav_path, const ts_recipe *recipe,
    const ts_rendered_sample *sample, const unsigned int fail_phase,
    ts_io_error *error)
{
    if (recipe_path == NULL || wav_path == NULL || recipe == NULL ||
        sample == NULL || strcmp(recipe_path, wav_path) == 0)
        return TS_IO_INVALID_ARGUMENT;
    char rs[1024], ws[1024], rb[1024], wb[1024];
    if (!unique_sibling(recipe_path, "replace", rs, sizeof rs) ||
        !unique_sibling(wav_path, "replace", ws, sizeof ws) ||
        !unique_sibling(recipe_path, "backup", rb, sizeof rb) ||
        !unique_sibling(wav_path, "backup", wb, sizeof wb))
        return TS_IO_OPEN_FAILED;
    ts_io_status status = ts_recipe_save_file(rs, recipe, error);
    if (status == TS_IO_OK) status = ts_wav_save_file(ws, sample, error);
    if (status != TS_IO_OK) { TS_UNLINK(rs); TS_UNLINK(ws); return status; }
    const bool had_r = path_exists(recipe_path), had_w = path_exists(wav_path);
    if ((had_r && rename(recipe_path, rb) != 0) ||
        (had_w && rename(wav_path, wb) != 0))
    {
        bool restored = true;
        if (had_r && !path_exists(recipe_path))
            restored = rename(rb, recipe_path) == 0;
        TS_UNLINK(rs); TS_UNLINK(ws);
        if (!restored) {
            char message[160];
            snprintf(message, sizeof message,
                "backup setup recovery incomplete: %.110s retained", rb);
            set_error(error, TS_IO_ROLLBACK_FAILED, 0, message);
            return TS_IO_ROLLBACK_FAILED;
        }
        set_error(error, TS_IO_RENAME_FAILED, 0,
            "backup setup failed; rollback completed");
        return TS_IO_RENAME_FAILED;
    }
    bool recipe_new = false, wav_new = false;
    if (fail_phase == 1U || !publish_no_replace(rs, recipe_path)) goto rollback;
    recipe_new = true;
    if (fail_phase == 2U || !publish_no_replace(ws, wav_path)) goto rollback;
    wav_new = true;
    if (fail_phase >= 3U) goto rollback;
    if (had_r) TS_UNLINK(rb);
    if (had_w) TS_UNLINK(wb);
    set_error(error, TS_IO_OK, 0, "ok"); return TS_IO_OK;
rollback:
    if (recipe_new) TS_UNLINK(recipe_path); else TS_UNLINK(rs);
    if (wav_new) TS_UNLINK(wav_path); else TS_UNLINK(ws);
    bool recipe_restored = !had_r;
    bool wav_restored = !had_w;
    if (had_r && fail_phase != 4U && fail_phase != 6U)
        recipe_restored = rename(rb, recipe_path) == 0;
    if (had_w && fail_phase != 5U && fail_phase != 6U)
        wav_restored = rename(wb, wav_path) == 0;
    if (!recipe_restored || !wav_restored)
    {
        char message[160]; const char *retained = !recipe_restored ? rb : wb;
        snprintf(message, sizeof message,
            "pair recovery incomplete: %.100s retained%s", retained,
            (!recipe_restored && !wav_restored) ? "; both backups" : "");
        set_error(error, TS_IO_ROLLBACK_FAILED, 0, message);
        return TS_IO_ROLLBACK_FAILED;
    }
    set_error(error, TS_IO_RENAME_FAILED, 0, "pair replacement rolled back");
    return TS_IO_RENAME_FAILED;
}

ts_io_status ts_bake_pair_replace_files(const char *recipe_path,
    const char *wav_path, const ts_recipe *recipe,
    const ts_rendered_sample *sample, ts_io_error *error)
{
    return ts_bake_pair_replace_files_test(recipe_path, wav_path, recipe,
        sample, 0U, error);
}
