// Buraaq standard library native runtime — zero-GC, explicit allocation.
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "buraaq_std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#else
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#ifdef BURAAQ_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif
#endif

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *buraaq_read_line(void);

char *buraaq_file_read(const char *path) {
    if (!path) return NULL;
    if (path[0] == '-' && path[1] == 0) {
        return buraaq_read_line();
    }
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

int buraaq_file_write(const char *path, const char *contents) {
    if (!path || !contents) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = strlen(contents);
    size_t w = fwrite(contents, 1, n, f);
    fclose(f);
    return w == n ? 0 : -1;
}

/* Append without reading the file back. Building output by
   `write(path, concat(read(path), line))` is quadratic in the output size; the
   bootstrap compiler emits ~30k lines that way, so it needs this. */
int buraaq_file_append(const char *path, const char *contents) {
    if (!path || !contents) return -1;
    FILE *f = fopen(path, "ab");
    if (!f) return -1;
    size_t n = strlen(contents);
    size_t w = fwrite(contents, 1, n, f);
    fclose(f);
    return w == n ? 0 : -1;
}

int buraaq_file_exists(const char *path) {
    if (!path) return 0;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

int buraaq_mkdir(const char *path) {
    if (!path || !path[0]) return -1;
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL)) return 0;
    return GetLastError() == ERROR_ALREADY_EXISTS ? 0 : -1;
#else
    if (mkdir(path, 0755) == 0) return 0;
    return errno == EEXIST ? 0 : -1;
#endif
}

int buraaq_chdir(const char *path) {
    if (!path || !path[0]) return -1;
#ifdef _WIN32
    return SetCurrentDirectoryA(path) ? 0 : -1;
#else
    return chdir(path) == 0 ? 0 : -1;
#endif
}

/* Every release in this file goes through these so the length cache in
   buraaq_rt.c cannot answer for a string that no longer lives at that address.
   Growing a buffer counts: realloc keeps the address as often as not, and a
   cached length taken before the growth would describe only part of it. */
static void bq_free(void *p) {
    buraaq_forget_length();
    free(p);
}

static void *bq_realloc(void *p, size_t n) {
    buraaq_forget_length();
    return realloc(p, n);
}

char *buraaq_text_concat(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = buraaq_length_of(a), lb = buraaq_length_of(b);
    char *out = (char *)malloc(la + lb + 1);
    if (!out) return NULL;
    memcpy(out, a, la);
    memcpy(out + la, b, lb + 1);
    return out;
}

int32_t buraaq_text_len(const char *s) { return (int32_t)buraaq_length_of(s); }

int32_t buraaq_text_eq(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b);
}

int32_t buraaq_text_lt(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) < 0 ? 1 : 0;
}

int32_t buraaq_text_byte(const char *s, int32_t i) {
    if (!s || i < 0) return -1;
    size_t n = buraaq_length_of(s);
    if ((size_t)i >= n) return -1;
    return (int32_t)(unsigned char)s[i];
}

char *buraaq_text_slice(const char *s, int32_t start, int32_t end) {
    if (!s) s = "";
    size_t n = buraaq_length_of(s);
    if (start < 0) start = 0;
    if (end < start) end = start;
    if ((size_t)start > n) start = (int32_t)n;
    if ((size_t)end > n) end = (int32_t)n;
    size_t len = (size_t)(end - start);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s + start, len);
    out[len] = '\0';
    return out;
}

int32_t buraaq_keepalive_i32(int32_t v) {
    static volatile int32_t sink;
    sink = v;
    return sink;
}

double buraaq_keepalive_f64(double v) {
    static volatile double sink;
    sink = v;
    return sink;
}

void buraaq_bench_report(const char *name, double sec, int32_t n, int32_t checksum) {
    static volatile int32_t sink;
    sink = checksum;
    if (!name) name = "bench";
    if (sec > 0.0) {
        printf("BENCH name=%s time_sec=%.6f ops=%d ops_per_sec=%.0f\n",
               name, sec, n, (double)n / sec);
    } else {
        printf("BENCH name=%s time_sec=%.6f ops=%d ops_per_sec=0\n", name, sec, n);
    }
}

void buraaq_bench_report_f64(const char *name, double sec, int32_t n, double checksum) {
    static volatile double sink;
    sink = checksum;
    buraaq_bench_report(name, sec, n, (int32_t)(checksum * 1000.0));
}

double buraaq_now_sec(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

double buraaq_math_sqrt(double x) { return sqrt(x); }
double buraaq_math_abs_f64(double x) { return fabs(x); }
int32_t buraaq_math_abs_i32(int32_t x) { return x < 0 ? -x : x; }
double buraaq_math_min_f64(double a, double b) { return a < b ? a : b; }
double buraaq_math_max_f64(double a, double b) { return a > b ? a : b; }
double buraaq_math_sin(double x) { return sin(x); }
double buraaq_math_cos(double x) { return cos(x); }
double buraaq_math_tan(double x) { return tan(x); }
double buraaq_math_asin(double x) { return asin(x); }
double buraaq_math_acos(double x) { return acos(x); }
double buraaq_math_atan(double x) { return atan(x); }
double buraaq_math_atan2(double y, double x) { return atan2(y, x); }
double buraaq_math_sinh(double x) { return sinh(x); }
double buraaq_math_cosh(double x) { return cosh(x); }
double buraaq_math_tanh(double x) { return tanh(x); }
double buraaq_math_exp(double x) { return exp(x); }
double buraaq_math_log(double x) { return log(x); }
double buraaq_math_log10(double x) { return log10(x); }
double buraaq_math_log2(double x) { return log(x) / log(2.0); }
double buraaq_math_pow(double x, double y) { return pow(x, y); }
double buraaq_math_hypot(double x, double y) { return hypot(x, y); }
double buraaq_math_floor(double x) { return floor(x); }
double buraaq_math_ceil(double x) { return ceil(x); }
double buraaq_math_trunc(double x) { return trunc(x); }
double buraaq_math_round(double x) { return round(x); }
double buraaq_math_fmod(double x, double y) { return fmod(x, y); }
double buraaq_math_copysign(double mag, double sgn) { return copysign(mag, sgn); }
double buraaq_math_cbrt(double x) { return cbrt(x); }
double buraaq_math_pi(void) { return 3.14159265358979323846; }
double buraaq_math_euler(void) { return 2.71828182845904523536; }
double buraaq_math_deg(double rad) { return rad * 180.0 / 3.14159265358979323846; }
double buraaq_math_rad(double deg) { return deg * 3.14159265358979323846 / 180.0; }
double buraaq_math_clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}
double buraaq_math_lerp(double a, double b, double t) { return a + (b - a) * t; }
double buraaq_math_sign(double x) {
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return 0.0;
}

int64_t buraaq_time_now_ms(void) {
    return (int64_t)time(NULL) * 1000;
}

void buraaq_time_sleep_ms(int64_t ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    {
        struct timespec ts;
        ts.tv_sec = (time_t)(ms / 1000);
        ts.tv_nsec = (long)((ms % 1000) * 1000000L);
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        }
    }
#endif
}

char *buraaq_os_getenv(const char *name) {
    if (!name) return NULL;
    const char *v = getenv(name);
    return v ? dup_str(v) : NULL;
}

int buraaq_rt_argc = 0;
char **buraaq_rt_argv = NULL;

void buraaq_rt_set_args(int argc, char **argv) {
    buraaq_rt_argc = argc;
    buraaq_rt_argv = argv;
}

int32_t buraaq_os_argc(void) { return (int32_t)buraaq_rt_argc; }
char *buraaq_os_argv(int32_t index) {
    if (index < 0 || index >= buraaq_rt_argc || !buraaq_rt_argv) return NULL;
    return dup_str(buraaq_rt_argv[index]);
}

static char *dup_range(const char *start, const char *end) {
    if (!start || end < start) return NULL;
    size_t n = (size_t)(end - start);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

char *buraaq_json_parse_string_field(const char *json, const char *key) {
    if (!json || !key || !key[0]) return NULL;
    char quoted[128];
    char bare[128];
    snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    snprintf(bare, sizeof(bare), "%s:", key);
    const char *p = strstr(json, quoted);
    if (p) {
        p = strchr(p + strlen(quoted), ':');
        if (!p) return NULL;
    } else {
        p = strstr(json, bare);
        if (!p) return NULL;
        if (p != json) {
            char prev = p[-1];
            if (prev != '{' && prev != ',' && prev != ' ' && prev != '\n' && prev != '\r' && prev != '\t') {
                return NULL;
            }
        }
    }
    while (*p && (*p == ':' || *p == ' ' || *p == '\t')) p++;
    if (*p == '"') {
        p++;
        const char *end = p;
        while (*end && *end != '"') {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        if (*end != '"') return NULL;
        return dup_range(p, end);
    }
    const char *end = p;
    while (*end && *end != ',' && *end != '}' && *end != ']' && *end != '\n' && *end != '\r') {
        end++;
    }
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) end--;
    if (end <= p) return NULL;
    return dup_range(p, end);
}

#define BQJ_NULL 0
#define BQJ_BOOL 1
#define BQJ_INT 2
#define BQJ_FLOAT 3
#define BQJ_STR 4
#define BQJ_ARR 5
#define BQJ_OBJ 6
#define BQJ_MAX_DEPTH 32

typedef struct BqJson BqJson;
struct BqJson {
    int32_t kind;
    int32_t ival;
    double fval;
    char *sval;
    BqJson **kids;
    char **keys;
    int32_t len;
    int32_t cap;
};

typedef struct {
    const char *s;
    size_t i;
    size_t n;
    int err;
    int depth;
} BqJp;

static int bqj_ws(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void bqj_skip(BqJp *p) {
    while (p->i < p->n && bqj_ws((unsigned char)p->s[p->i])) p->i++;
}

static BqJson *bqj_new(int kind) {
    BqJson *v = (BqJson *)calloc(1, sizeof(BqJson));
    if (v) v->kind = kind;
    return v;
}

static void bqj_push(BqJson *arr, BqJson *kid, char *key) {
    if (!arr || !kid) {
        if (key) free(key);
        return;
    }
    if (arr->len >= arr->cap) {
        int32_t ncap = arr->cap ? arr->cap * 2 : 4;
        BqJson **nk = (BqJson **)realloc(arr->kids, (size_t)ncap * sizeof(BqJson *));
        if (!nk) return;
        arr->kids = nk;
        if (arr->kind == BQJ_OBJ) {
            char **kk = (char **)realloc(arr->keys, (size_t)ncap * sizeof(char *));
            if (!kk) return;
            arr->keys = kk;
        }
        arr->cap = ncap;
    }
    arr->kids[arr->len] = kid;
    if (arr->kind == BQJ_OBJ) arr->keys[arr->len] = key;
    arr->len++;
}

static char *bqj_str(BqJp *p);
static BqJson *bqj_val(BqJp *p);

static char *bqj_str(BqJp *p) {
    if (p->i >= p->n || p->s[p->i] != '"') {
        p->err = 1;
        return NULL;
    }
    p->i++;
    size_t cap = 16, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) {
        p->err = 1;
        return NULL;
    }
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i++];
        if (c == '"') {
            out[len] = 0;
            return out;
        }
        if (c == '\\') {
            if (p->i >= p->n) break;
            c = (unsigned char)p->s[p->i++];
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c == 'u') {
                unsigned int cp = 0;
                int k;
                for (k = 0; k < 4 && p->i < p->n; k++) {
                    char h = p->s[p->i++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                    else {
                        p->err = 1;
                        free(out);
                        return NULL;
                    }
                }
                if (cp < 0x80) c = (unsigned char)cp;
                else if (cp < 0x800) {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        char *n = (char *)realloc(out, cap);
                        if (!n) {
                            p->err = 1;
                            free(out);
                            return NULL;
                        }
                        out = n;
                    }
                    out[len++] = (char)(0xC0 | (cp >> 6));
                    out[len++] = (char)(0x80 | (cp & 0x3F));
                    continue;
                } else {
                    if (len + 3 >= cap) {
                        cap *= 2;
                        char *n = (char *)realloc(out, cap);
                        if (!n) {
                            p->err = 1;
                            free(out);
                            return NULL;
                        }
                        out = n;
                    }
                    out[len++] = (char)(0xE0 | (cp >> 12));
                    out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[len++] = (char)(0x80 | (cp & 0x3F));
                    continue;
                }
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char *n = (char *)realloc(out, cap);
            if (!n) {
                p->err = 1;
                free(out);
                return NULL;
            }
            out = n;
        }
        out[len++] = (char)c;
    }
    p->err = 1;
    free(out);
    return NULL;
}

static BqJson *bqj_num(BqJp *p) {
    const char *start = p->s + p->i;
    int is_float = 0;
    if (p->i < p->n && (p->s[p->i] == '-' || p->s[p->i] == '+')) p->i++;
    while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    if (p->i < p->n && p->s[p->i] == '.') {
        is_float = 1;
        p->i++;
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    }
    if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        is_float = 1;
        p->i++;
        if (p->i < p->n && (p->s[p->i] == '-' || p->s[p->i] == '+')) p->i++;
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    }
    BqJson *v = bqj_new(is_float ? BQJ_FLOAT : BQJ_INT);
    if (!v) {
        p->err = 1;
        return NULL;
    }
    if (is_float) {
        v->fval = strtod(start, NULL);
        v->ival = (int32_t)v->fval;
    } else {
        v->ival = (int32_t)strtol(start, NULL, 10);
        v->fval = (double)v->ival;
    }
    return v;
}

static BqJson *bqj_val(BqJp *p) {
    bqj_skip(p);
    if (p->err || p->i >= p->n) {
        p->err = 1;
        return NULL;
    }
    if (p->depth > BQJ_MAX_DEPTH) {
        p->err = 1;
        return NULL;
    }
    char c = p->s[p->i];
    if (c == '"') {
        BqJson *v = bqj_new(BQJ_STR);
        if (!v) {
            p->err = 1;
            return NULL;
        }
        v->sval = bqj_str(p);
        if (p->err) return v;
        return v;
    }
    if (c == '{') {
        p->i++;
        p->depth++;
        BqJson *v = bqj_new(BQJ_OBJ);
        if (!v) {
            p->err = 1;
            return NULL;
        }
        bqj_skip(p);
        if (p->i < p->n && p->s[p->i] == '}') {
            p->i++;
            p->depth--;
            return v;
        }
        while (p->i < p->n && !p->err) {
            bqj_skip(p);
            char *key = bqj_str(p);
            bqj_skip(p);
            if (p->i >= p->n || p->s[p->i] != ':') {
                p->err = 1;
                free(key);
                break;
            }
            p->i++;
            BqJson *kid = bqj_val(p);
            bqj_push(v, kid, key);
            bqj_skip(p);
            if (p->i < p->n && p->s[p->i] == ',') {
                p->i++;
                continue;
            }
            if (p->i < p->n && p->s[p->i] == '}') {
                p->i++;
                break;
            }
            p->err = 1;
            break;
        }
        p->depth--;
        return v;
    }
    if (c == '[') {
        p->i++;
        p->depth++;
        BqJson *v = bqj_new(BQJ_ARR);
        if (!v) {
            p->err = 1;
            return NULL;
        }
        bqj_skip(p);
        if (p->i < p->n && p->s[p->i] == ']') {
            p->i++;
            p->depth--;
            return v;
        }
        while (p->i < p->n && !p->err) {
            BqJson *kid = bqj_val(p);
            bqj_push(v, kid, NULL);
            bqj_skip(p);
            if (p->i < p->n && p->s[p->i] == ',') {
                p->i++;
                continue;
            }
            if (p->i < p->n && p->s[p->i] == ']') {
                p->i++;
                break;
            }
            p->err = 1;
            break;
        }
        p->depth--;
        return v;
    }
    if (c == 't' && p->i + 4 <= p->n && memcmp(p->s + p->i, "true", 4) == 0) {
        p->i += 4;
        BqJson *v = bqj_new(BQJ_BOOL);
        if (v) v->ival = 1;
        return v;
    }
    if (c == 'f' && p->i + 5 <= p->n && memcmp(p->s + p->i, "false", 5) == 0) {
        p->i += 5;
        return bqj_new(BQJ_BOOL);
    }
    if (c == 'n' && p->i + 4 <= p->n && memcmp(p->s + p->i, "null", 4) == 0) {
        p->i += 4;
        return bqj_new(BQJ_NULL);
    }
    if (c == '-' || c == '+' || (c >= '0' && c <= '9')) return bqj_num(p);
    p->err = 1;
    return NULL;
}

void *buraaq_json_parse(const char *text) {
    if (!text) return NULL;
    BqJp p;
    p.s = text;
    p.i = 0;
    p.n = strlen(text);
    p.err = 0;
    p.depth = 0;
    BqJson *v = bqj_val(&p);
    bqj_skip(&p);
    if (p.err || p.i != p.n) return v;
    return v;
}

static int bqj_put(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 >= *cap) {
        size_t ncap = *cap ? *cap : 64;
        while (*len + n + 1 >= ncap) ncap *= 2;
        char *nb = (char *)realloc(*buf, ncap);
        if (!nb) return 0;
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = 0;
    return 1;
}

static int bqj_puts(char **buf, size_t *len, size_t *cap, const char *s) {
    return bqj_put(buf, len, cap, s, s ? strlen(s) : 0);
}

static void bqj_esc(char **buf, size_t *len, size_t *cap, const char *s) {
    bqj_puts(buf, len, cap, "\"");
    if (!s) {
        bqj_puts(buf, len, cap, "\"");
        return;
    }
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            char t[3] = {'\\', (char)c, 0};
            bqj_puts(buf, len, cap, t);
        } else if (c == '\n') {
            bqj_puts(buf, len, cap, "\\n");
        } else if (c == '\t') {
            bqj_puts(buf, len, cap, "\\t");
        } else if (c == '\r') {
            bqj_puts(buf, len, cap, "\\r");
        } else {
            char t[2] = {(char)c, 0};
            bqj_puts(buf, len, cap, t);
        }
    }
    bqj_puts(buf, len, cap, "\"");
}

static void bqj_write(const BqJson *v, char **buf, size_t *len, size_t *cap) {
    char tmp[64];
    int32_t i;
    if (!v) {
        bqj_puts(buf, len, cap, "null");
        return;
    }
    if (v->kind == BQJ_NULL) {
        bqj_puts(buf, len, cap, "null");
    } else if (v->kind == BQJ_BOOL) {
        bqj_puts(buf, len, cap, v->ival ? "true" : "false");
    } else if (v->kind == BQJ_INT) {
        snprintf(tmp, sizeof(tmp), "%d", (int)v->ival);
        bqj_puts(buf, len, cap, tmp);
    } else if (v->kind == BQJ_FLOAT) {
        snprintf(tmp, sizeof(tmp), "%.15g", v->fval);
        bqj_puts(buf, len, cap, tmp);
    } else if (v->kind == BQJ_STR) {
        bqj_esc(buf, len, cap, v->sval);
    } else if (v->kind == BQJ_ARR) {
        bqj_puts(buf, len, cap, "[");
        for (i = 0; i < v->len; i++) {
            if (i) bqj_puts(buf, len, cap, ",");
            bqj_write(v->kids[i], buf, len, cap);
        }
        bqj_puts(buf, len, cap, "]");
    } else if (v->kind == BQJ_OBJ) {
        bqj_puts(buf, len, cap, "{");
        for (i = 0; i < v->len; i++) {
            if (i) bqj_puts(buf, len, cap, ",");
            bqj_esc(buf, len, cap, v->keys[i]);
            bqj_puts(buf, len, cap, ":");
            bqj_write(v->kids[i], buf, len, cap);
        }
        bqj_puts(buf, len, cap, "}");
    }
}

char *buraaq_json_stringify(const void *vp) {
    char *buf = NULL;
    size_t len = 0, cap = 0;
    bqj_write((const BqJson *)vp, &buf, &len, &cap);
    if (!buf) return dup_str("null");
    return buf;
}

void *buraaq_json_get(const void *vp, const char *key) {
    const BqJson *v = (const BqJson *)vp;
    int32_t i;
    if (!v || v->kind != BQJ_OBJ || !key) return NULL;
    for (i = 0; i < v->len; i++) {
        if (v->keys[i] && strcmp(v->keys[i], key) == 0) return v->kids[i];
    }
    return NULL;
}

void *buraaq_json_item(const void *vp, int32_t index) {
    const BqJson *v = (const BqJson *)vp;
    if (!v || v->kind != BQJ_ARR || index < 0 || index >= v->len) return NULL;
    return v->kids[index];
}

char *buraaq_json_field(const void *vp, const char *key) {
    BqJson *kid = (BqJson *)buraaq_json_get(vp, key);
    if (!kid) return NULL;
    if (kid->kind == BQJ_STR) return dup_str(kid->sval ? kid->sval : "");
    return buraaq_json_stringify(kid);
}

int32_t buraaq_json_as_int(const void *vp) {
    const BqJson *v = (const BqJson *)vp;
    if (!v) return 0;
    if (v->kind == BQJ_INT || v->kind == BQJ_BOOL) return v->ival;
    if (v->kind == BQJ_FLOAT) return (int32_t)v->fval;
    if (v->kind == BQJ_STR && v->sval) return (int32_t)strtol(v->sval, NULL, 10);
    return 0;
}

char *buraaq_json_as_text(const void *vp) {
    const BqJson *v = (const BqJson *)vp;
    if (!v) return NULL;
    if (v->kind == BQJ_STR) return dup_str(v->sval ? v->sval : "");
    return buraaq_json_stringify(v);
}

int32_t buraaq_json_kind(const void *vp) {
    const BqJson *v = (const BqJson *)vp;
    return v ? v->kind : BQJ_NULL;
}

int32_t buraaq_json_count(const void *vp) {
    const BqJson *v = (const BqJson *)vp;
    if (!v) return 0;
    if (v->kind == BQJ_ARR || v->kind == BQJ_OBJ) return v->len;
    return 0;
}

static const BqJson *bqj_path(const BqJson *v, const char *path) {
    char key[128];
    size_t k;
    if (!v || !path) return NULL;
    while (*path && v) {
        k = 0;
        while (path[k] && path[k] != '.' && k + 1 < sizeof(key)) {
            key[k] = path[k];
            k++;
        }
        key[k] = 0;
        if (v->kind == BQJ_OBJ) v = (const BqJson *)buraaq_json_get(v, key);
        else if (v->kind == BQJ_ARR) v = (const BqJson *)buraaq_json_item(v, (int32_t)strtol(key, NULL, 10));
        else return NULL;
        if (!path[k]) break;
        path += k + 1;
    }
    return v;
}

int32_t buraaq_json_path_int(const void *v, const char *path) {
    return buraaq_json_as_int(bqj_path((const BqJson *)v, path));
}

char *buraaq_json_path_text(const void *v, const char *path) {
    return buraaq_json_as_text(bqj_path((const BqJson *)v, path));
}

#ifdef _WIN32
static char *http_get_wininet(const char *url) {
    HINTERNET ses = InternetOpenA("buraaq/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!ses) return NULL;
    HINTERNET req = InternetOpenUrlA(ses, url, NULL, 0, INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD, 0);
    if (!req) {
        InternetCloseHandle(ses);
        return NULL;
    }
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        InternetCloseHandle(req);
        InternetCloseHandle(ses);
        return NULL;
    }
    DWORD n = 0;
    while (InternetReadFile(req, buf + len, (DWORD)(cap - len - 1), &n) && n > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)bq_realloc(buf, cap);
            if (!nb) {
                bq_free(buf);
                InternetCloseHandle(req);
                InternetCloseHandle(ses);
                return NULL;
            }
            buf = nb;
        }
    }
    buf[len] = '\0';
    InternetCloseHandle(req);
    InternetCloseHandle(ses);
    return buf;
}
#else
static char *http_strip_headers(char *buf) {
    if (!buf) return NULL;
    char *body = strstr(buf, "\r\n\r\n");
    if (body) {
        body += 4;
        char *out = dup_str(body);
        bq_free(buf);
        return out;
    }
    return buf;
}

#ifdef BURAAQ_OPENSSL
static char *http_tls_exchange(int fd, const char *host, const char *req) {
    SSL_library_init();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    if (!method) {
        close(fd);
        return NULL;
    }
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        close(fd);
        return NULL;
    }
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        close(fd);
        return NULL;
    }
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, host);
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return NULL;
    }
    long vr = SSL_get_verify_result(ssl);
    if (vr != X509_V_OK) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return NULL;
    }
    if (SSL_write(ssl, req, (int)strlen(req)) <= 0) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return NULL;
    }
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return NULL;
    }
    for (;;) {
        int n = SSL_read(ssl, buf + len, (int)(cap - len - 1));
        if (n <= 0) break;
        len += (size_t)n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)bq_realloc(buf, cap);
            if (!nb) {
                bq_free(buf);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                close(fd);
                return NULL;
            }
            buf = nb;
        }
    }
    buf[len] = '\0';
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);
    return http_strip_headers(buf);
}
#endif

static char *http_get_posix(const char *url) {
    const char *p = url;
    int https = 0;
    if (strncmp(p, "https://", 8) == 0) {
        https = 1;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else {
        return NULL;
    }
    char host[256];
    int port = https ? 443 : 80;
    const char *path = strchr(p, '/');
    size_t host_len = path ? (size_t)(path - p) : strlen(p);
    const char *colon = memchr(p, ':', host_len);
    if (colon) {
        host_len = (size_t)(colon - p);
        port = atoi(colon + 1);
    }
    if (host_len >= sizeof(host)) return NULL;
    memcpy(host, p, host_len);
    host[host_len] = '\0';
    if (!path) path = "/";

#ifndef BURAAQ_OPENSSL
    if (https) return NULL;
#endif

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    char port_s[16];
    snprintf(port_s, sizeof(port_s), "%d", port);
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_s, &hints, &res) != 0 || !res) return NULL;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return NULL;
    }
    if (connect(fd, res->ai_addr, (socklen_t)res->ai_addrlen) != 0) {
        close(fd);
        freeaddrinfo(res);
        return NULL;
    }
    freeaddrinfo(res);
    char req[1024];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: buraaq/1.0\r\n\r\n", path, host);
#ifdef BURAAQ_OPENSSL
    if (https) {
        return http_tls_exchange(fd, host, req);
    }
#endif
    if (send(fd, req, strlen(req), 0) < 0) {
        close(fd);
        return NULL;
    }
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        close(fd);
        return NULL;
    }
    for (;;) {
        ssize_t n = recv(fd, buf + len, cap - len - 1, 0);
        if (n <= 0) break;
        len += (size_t)n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)bq_realloc(buf, cap);
            if (!nb) {
                bq_free(buf);
                close(fd);
                return NULL;
            }
            buf = nb;
        }
    }
    close(fd);
    buf[len] = '\0';
    return http_strip_headers(buf);
}
#endif

char *buraaq_http_get_body(const char *url) {
    if (!url) return NULL;
    if (strncmp(url, "file://", 7) == 0) {
        const char *path = url + 7;
        /* Only the empty authority is local: file://host/share names a remote
           share, and file:////host/share smuggles one past a single-slash check. */
        if (path[0] != '/' || path[1] == '/' || path[1] == '\\') return NULL;
#ifdef _WIN32
        /* file:///C:/x addresses drive path C:/x; file:///x stays rooted. */
        if (path[1] && path[2] == ':') path++;
#endif
        /* Defence in depth against traversal and UNC smuggling. */
        if (strstr(path, "..") != NULL || strstr(path, "\\\\") != NULL) return NULL;
        return buraaq_file_read(path);
    }
#ifdef _WIN32
    char *body = http_get_wininet(url);
#else
    char *body = http_get_posix(url);
#endif
    if (body) return body;
    return dup_str("{\"status\":0,\"body\":\"http get failed\"}");
}

static uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

char *buraaq_crypto_sha256_hex(const char *data) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    const uint8_t *msg = (const uint8_t *)(data ? data : "");
    uint64_t bitlen = (uint64_t)strlen((const char *)msg) * 8;
    size_t len = (size_t)(bitlen / 8);
    size_t pad = ((len + 9 + 63) / 64) * 64;
    uint8_t *buf = (uint8_t *)calloc(pad, 1);
    if (!buf) return NULL;
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    for (int i = 0; i < 8; i++) buf[pad - 1 - i] = (uint8_t)(bitlen >> (8 * i));
    for (size_t off = 0; off < pad; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)buf[off + i * 4] << 24) | ((uint32_t)buf[off + i * 4 + 1] << 16)
                | ((uint32_t)buf[off + i * 4 + 2] << 8) | (uint32_t)buf[off + i * 4 + 3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    bq_free(buf);
    char *out = (char *)malloc(65);
    if (!out) return NULL;
    for (int i = 0; i < 8; i++) sprintf(out + i * 8, "%08x", h[i]);
    out[64] = '\0';
    return out;
}

void *buraaq_mutex_new(void) {
#ifdef _WIN32
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
    if (!cs) return NULL;
    InitializeCriticalSection(cs);
    return cs;
#else
    pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!m) return NULL;
    pthread_mutex_init(m, NULL);
    return m;
#endif
}

void buraaq_mutex_lock(void *m) {
    if (!m) return;
#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION *)m);
#else
    pthread_mutex_lock((pthread_mutex_t *)m);
#endif
}

void buraaq_mutex_unlock(void *m) {
    if (!m) return;
#ifdef _WIN32
    LeaveCriticalSection((CRITICAL_SECTION *)m);
#else
    pthread_mutex_unlock((pthread_mutex_t *)m);
#endif
}

void buraaq_mutex_free(void *m) {
    if (!m) return;
#ifdef _WIN32
    DeleteCriticalSection((CRITICAL_SECTION *)m);
#else
    pthread_mutex_destroy((pthread_mutex_t *)m);
#endif
    bq_free(m);
}

/* --- process argv builder ---------------------------------------------
   `buraaq_process_exit_code` takes one string, so it cannot pass arguments and
   has to refuse anything that looks like shell syntax. These build an argument
   vector instead and hand it straight to CreateProcess/execvp, so no shell is
   involved and metacharacters need no filtering. Handles keep the surface to
   int and text, which is all the bootstrap compiler can express. */

#define BQ_ARGV_SLOTS 32
#define BQ_ARGV_ARGS 256

typedef struct {
    int in_use;
    int count;
    char *args[BQ_ARGV_ARGS];
} BqArgv;

static BqArgv g_argv[BQ_ARGV_SLOTS];

static BqArgv *argv_get(int32_t h) {
    if (h < 0 || h >= BQ_ARGV_SLOTS || !g_argv[h].in_use) return NULL;
    return &g_argv[h];
}

int32_t buraaq_argv_new(void) {
    for (int i = 0; i < BQ_ARGV_SLOTS; i++) {
        if (!g_argv[i].in_use) {
            g_argv[i].in_use = 1;
            g_argv[i].count = 0;
            return i;
        }
    }
    return -1;
}

int32_t buraaq_argv_push(int32_t h, const char *arg) {
    BqArgv *a = argv_get(h);
    if (!a || !arg || a->count >= BQ_ARGV_ARGS) return -1;
    char *copy = dup_str(arg);
    if (!copy) return -1;
    a->args[a->count++] = copy;
    return a->count;
}

void buraaq_argv_free(int32_t h) {
    BqArgv *a = argv_get(h);
    if (!a) return;
    for (int i = 0; i < a->count; i++) bq_free(a->args[i]);
    a->count = 0;
    a->in_use = 0;
}

#ifdef _WIN32
typedef struct {
    char *p;
    size_t len;
    size_t cap;
} BqCmdLine;

static int cl_push(BqCmdLine *b, char c) {
    if (b->len + 2 > b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 256;
        char *np = (char *)bq_realloc(b->p, ncap);
        if (!np) return 0;
        b->p = np;
        b->cap = ncap;
    }
    b->p[b->len++] = c;
    b->p[b->len] = '\0';
    return 1;
}

/* Quote one argument so CommandLineToArgvW recovers it byte for byte. */
static int cl_quote(BqCmdLine *b, const char *arg) {
    int needs = (arg[0] == '\0');
    for (const char *p = arg; *p && !needs; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v' || *p == '"') needs = 1;
    }
    if (!needs) {
        for (const char *p = arg; *p; p++) {
            if (!cl_push(b, *p)) return 0;
        }
        return 1;
    }
    if (!cl_push(b, '"')) return 0;
    for (const char *p = arg;; p++) {
        unsigned slashes = 0;
        while (*p == '\\') {
            slashes++;
            p++;
        }
        if (*p == '\0') {
            /* Backslashes before the closing quote must be doubled. */
            for (unsigned i = 0; i < slashes * 2; i++) {
                if (!cl_push(b, '\\')) return 0;
            }
            break;
        }
        if (*p == '"') {
            for (unsigned i = 0; i < slashes * 2 + 1; i++) {
                if (!cl_push(b, '\\')) return 0;
            }
            if (!cl_push(b, '"')) return 0;
        } else {
            for (unsigned i = 0; i < slashes; i++) {
                if (!cl_push(b, '\\')) return 0;
            }
            if (!cl_push(b, *p)) return 0;
        }
    }
    return cl_push(b, '"');
}
#endif

int32_t buraaq_process_run(int32_t h) {
    BqArgv *a = argv_get(h);
    if (!a || a->count == 0) return -1;
#ifdef _WIN32
    {
        BqCmdLine cl;
        memset(&cl, 0, sizeof(cl));
        for (int i = 0; i < a->count; i++) {
            if (i && !cl_push(&cl, ' ')) {
                bq_free(cl.p);
                return -1;
            }
            if (!cl_quote(&cl, a->args[i])) {
                bq_free(cl.p);
                return -1;
            }
        }
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        BOOL ok = CreateProcessA(NULL, cl.p, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        bq_free(cl.p);
        if (!ok) return -1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int32_t)code;
    }
#else
    {
        char *argv[BQ_ARGV_ARGS + 1];
        for (int i = 0; i < a->count; i++) argv[i] = a->args[i];
        argv[a->count] = NULL;
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            execvp(argv[0], argv);
            _exit(127);
        }
        int st = 0;
        if (waitpid(pid, &st, 0) < 0) return -1;
        if (WIFEXITED(st)) return (int32_t)WEXITSTATUS(st);
        return -1;
    }
#endif
}

int32_t buraaq_process_exit_code(const char *cmd) {
    if (!cmd || !cmd[0]) return -1;
    /* No shell: refuse metacharacters; run the program path only. */
    for (const char *p = cmd; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '|' || c == '&' || c == ';' || c == '`' || c == '$' || c == '>' || c == '<'
            || c == '\n' || c == '\r' || c == '"' || c == '\'' || c == '\\') {
            return -1;
        }
#ifdef _WIN32
        if (c == '%') return -1;
#endif
    }
    if (strstr(cmd, "..") != NULL) return -1;
#ifdef _WIN32
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        if (!CreateProcessA(cmd, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            return -1;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int32_t)code;
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            char *argv[] = {(char *)cmd, NULL};
            execv(cmd, argv);
            _exit(127);
        }
        int st = 0;
        if (waitpid(pid, &st, 0) < 0) return -1;
        if (WIFEXITED(st)) return (int32_t)WEXITSTATUS(st);
        return -1;
    }
#endif
}

#ifndef _WIN32
#define BUR_HDR_EQ(line) (strncasecmp((line), "Content-Length:", 15) == 0)
#else
#define BUR_HDR_EQ(line) (_strnicmp((line), "Content-Length:", 15) == 0)
#endif

char *buraaq_lsp_read(void) {
    char line[256];
    size_t need = 0;
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) return NULL;
        if (line[0] == '\r' || line[0] == '\n') break;
        if (BUR_HDR_EQ(line)) {
            const char *p = line + 15;
            while (*p == ' ') p++;
            need = (size_t)strtoul(p, NULL, 10);
        }
    }
    if (need == 0) return dup_str("");
    char *buf = (char *)malloc(need + 1);
    if (!buf) return NULL;
    size_t got = 0;
    while (got < need) {
        size_t n = fread(buf + got, 1, need - got, stdin);
        if (n == 0) break;
        got += n;
    }
    buf[got] = 0;
    return buf;
}

void buraaq_lsp_write(const char *json) {
    if (!json) json = "{}";
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", strlen(json), json);
    fflush(stdout);
}

static int bur_put_u32(FILE *fp, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 255);
    b[1] = (unsigned char)((v >> 8) & 255);
    b[2] = (unsigned char)((v >> 16) & 255);
    b[3] = (unsigned char)((v >> 24) & 255);
    return fwrite(b, 1, 4, fp) == 4;
}

static int bur_add_file(FILE *fp, const char *name, const char *path) {
    FILE *in;
    long sz;
    char *buf;
    uint32_t nlen, dlen;
    if (!path || !buraaq_file_exists(path)) return 0;
    in = fopen(path, "rb");
    if (!in) return 0;
    if (fseek(in, 0, SEEK_END) != 0) { fclose(in); return 0; }
    sz = ftell(in);
    if (sz < 0) { fclose(in); return 0; }
    rewind(in);
    buf = (char *)malloc(sz > 0 ? (size_t)sz : 1);
    if (!buf) { fclose(in); return 0; }
    if (sz > 0 && fread(buf, 1, (size_t)sz, in) != (size_t)sz) {
        free(buf);
        fclose(in);
        return 0;
    }
    fclose(in);
    nlen = (uint32_t)strlen(name);
    dlen = (uint32_t)sz;
    if (!bur_put_u32(fp, nlen) || fwrite(name, 1, nlen, fp) != nlen) {
        free(buf);
        return 0;
    }
    if (!bur_put_u32(fp, dlen) || (dlen && fwrite(buf, 1, dlen, fp) != dlen)) {
        free(buf);
        return 0;
    }
    free(buf);
    return 1;
}

int32_t buraaq_bur_pack(const char *exe, const char *pkg, const char *out_path) {
    FILE *fp;
    if (!exe || !out_path) return 1;
    fp = fopen(out_path, "wb");
    if (!fp) return 1;
    if (fwrite("BUR1", 1, 4, fp) != 4) { fclose(fp); return 1; }
    if (!bur_add_file(fp, "bin/app", exe)) { fclose(fp); return 1; }
    if (pkg && buraaq_file_exists(pkg)) {
        if (!bur_add_file(fp, "buraaq.pkg", pkg)) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}

static int bur_mkdir_p(const char *path) {
    char buf[1024];
    size_t n = strlen(path);
    size_t i;
    if (n >= sizeof(buf)) return 0;
    memcpy(buf, path, n + 1);
    for (i = 1; i < n; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char c = buf[i];
            buf[i] = 0;
            buraaq_mkdir(buf);
            buf[i] = c;
        }
    }
    buraaq_mkdir(buf);
    return 1;
}

int32_t buraaq_bur_launch(const char *bur_path) {
    FILE *fp;
    char magic[4];
    char dest[1024];
    char exe[1024];
    exe[0] = 0;
    if (!bur_path) return 1;
    fp = fopen(bur_path, "rb");
    if (!fp) return 1;
    if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, "BUR1", 4) != 0) {
        fclose(fp);
        return 1;
    }
#ifdef _WIN32
    snprintf(dest, sizeof(dest), "%s\\.buraaq\\run\\ship", getenv("USERPROFILE") ? getenv("USERPROFILE") : ".");
#else
    snprintf(dest, sizeof(dest), "%s/.buraaq/run/ship", getenv("HOME") ? getenv("HOME") : ".");
#endif
    bur_mkdir_p(dest);
    for (;;) {
        unsigned char nb[4];
        uint32_t nlen, dlen;
        char name[256];
        char outp[1200];
        char *data;
        if (fread(nb, 1, 4, fp) != 4) break;
        nlen = (uint32_t)nb[0] | ((uint32_t)nb[1] << 8) | ((uint32_t)nb[2] << 16) | ((uint32_t)nb[3] << 24);
        if (nlen == 0 || nlen >= sizeof(name)) break;
        if (fread(name, 1, nlen, fp) != nlen) break;
        name[nlen] = 0;
        if (strstr(name, "..") || name[0] == '/' || name[0] == '\\') break;
        if (fread(nb, 1, 4, fp) != 4) break;
        dlen = (uint32_t)nb[0] | ((uint32_t)nb[1] << 8) | ((uint32_t)nb[2] << 16) | ((uint32_t)nb[3] << 24);
        data = (char *)malloc(dlen + 1);
        if (!data) break;
        if (dlen && fread(data, 1, dlen, fp) != dlen) { free(data); break; }
        snprintf(outp, sizeof(outp), "%s/%s", dest, name);
        {
            char *slash = strrchr(outp, '/');
#ifdef _WIN32
            char *bslash = strrchr(outp, '\\');
            if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
            if (slash) {
                char c = *slash;
                *slash = 0;
                bur_mkdir_p(outp);
                *slash = c;
            }
        }
        {
            FILE *out = fopen(outp, "wb");
            if (out) {
                if (dlen) fwrite(data, 1, dlen, out);
                fclose(out);
            }
        }
        if (strncmp(name, "bin/", 4) == 0) snprintf(exe, sizeof(exe), "%s", outp);
        free(data);
    }
    fclose(fp);
    if (!exe[0]) return 1;
#ifdef _WIN32
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));
        if (!CreateProcessA(exe, NULL, NULL, NULL, FALSE, 0, NULL, dest, &si, &pi)) return 1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0) return 1;
        if (pid == 0) {
            char *argv[] = {exe, NULL};
            execv(exe, argv);
            _exit(127);
        }
        int st = 0;
        waitpid(pid, &st, 0);
        return WIFEXITED(st) ? (int32_t)WEXITSTATUS(st) : 1;
    }
#endif
}

#ifdef _WIN32
int32_t buraaq_http_put_file(const char *url, const char *path, const char *token) {
    HINTERNET ses, con, req;
    URL_COMPONENTSA uc;
    char host[256], extra[1024];
    FILE *fp;
    long sz;
    char *body;
    char hdr[256];
    DWORD status = 0, slen = sizeof(status);
    INTERNET_PORT port;
    if (!url || !path) return 1;
    fp = fopen(path, "rb");
    if (!fp) return 1;
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    rewind(fp);
    body = (char *)malloc((size_t)(sz > 0 ? sz : 1));
    if (!body) { fclose(fp); return 1; }
    if (sz > 0) fread(body, 1, (size_t)sz, fp);
    fclose(fp);
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof(host);
    uc.lpszUrlPath = extra;
    uc.dwUrlPathLength = sizeof(extra);
    if (!InternetCrackUrlA(url, 0, 0, &uc)) { free(body); return 1; }
    port = uc.nPort;
    ses = InternetOpenA("buraaq/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!ses) { free(body); return 1; }
    con = InternetConnectA(ses, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!con) { InternetCloseHandle(ses); free(body); return 1; }
    req = HttpOpenRequestA(con, "PUT", extra[0] ? extra : "/", NULL, NULL, NULL,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0), 0);
    if (!req) { InternetCloseHandle(con); InternetCloseHandle(ses); free(body); return 1; }
    snprintf(hdr, sizeof(hdr), "Authorization: Bearer %s\r\nContent-Type: application/octet-stream\r\n", token ? token : "");
    if (!HttpSendRequestA(req, hdr, (DWORD)strlen(hdr), body, (DWORD)sz)) {
        InternetCloseHandle(req);
        InternetCloseHandle(con);
        InternetCloseHandle(ses);
        free(body);
        return 1;
    }
    HttpQueryInfoA(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &slen, NULL);
    InternetCloseHandle(req);
    InternetCloseHandle(con);
    InternetCloseHandle(ses);
    free(body);
    return status >= 200 && status < 300 ? 0 : 1;
}
#else
int32_t buraaq_http_put_file(const char *url, const char *path, const char *token) {
    (void)url;
    (void)path;
    (void)token;
    return 1;
}
#endif

#ifdef _WIN32
typedef SOCKET bq_hsock;
#define BQ_HINV INVALID_SOCKET
static int bq_hclose(bq_hsock s) { return closesocket(s) == 0 ? 0 : -1; }
#else
typedef int bq_hsock;
#define BQ_HINV (-1)
static int bq_hclose(bq_hsock s) { return close(s); }
#endif

static int bq_hstart(void) {
#ifdef _WIN32
    static int ready = 0;
    WSADATA wsa;
    if (ready) return 1;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
    ready = 1;
#endif
    return 1;
}

static void bq_home_join(char *out, size_t cap, const char *rel) {
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home || !home[0]) home = getenv("USERPROFILE");
#endif
    if (!home || !home[0]) home = ".";
    snprintf(out, cap, "%s/%s", home, rel);
}

static int bq_app_name_ok(const char *s) {
    size_t i;
    if (!s || !s[0]) return 0;
    if (!( (s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return 0;
    for (i = 1; s[i]; i++) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') continue;
        return 0;
    }
    return 1;
}

static int bq_tok_eq(const char *a, const char *b) {
    size_t na = a ? strlen(a) : 0;
    size_t nb = b ? strlen(b) : 0;
    size_t n = na > nb ? na : nb;
    unsigned char x = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char ca = i < na ? (unsigned char)a[i] : 0;
        unsigned char cb = i < nb ? (unsigned char)b[i] : 0;
        x = (unsigned char)(x | (ca ^ cb));
    }
    return x == 0 && na == nb;
}

static void bq_dock_token(char *out, size_t cap) {
    const char *env = getenv("BURAAQ_DOCK_TOKEN");
    char path[1024];
    char *got;
    if (env && env[0]) {
        snprintf(out, cap, "%s", env);
        return;
    }
    bq_home_join(path, sizeof(path), ".buraaq/dock/token");
    got = buraaq_file_read(path);
    if (got && got[0]) {
        size_t n = strlen(got);
        while (n > 0 && (got[n - 1] == '\n' || got[n - 1] == '\r')) {
            got[--n] = 0;
        }
        snprintf(out, cap, "%s", got);
        free(got);
        return;
    }
    free(got);
    {
        unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)out;
        size_t i;
        static const char hex[] = "0123456789abcdef";
        if (cap < 33) {
            out[0] = 0;
            return;
        }
        for (i = 0; i < 32; i++) {
            seed = seed * 1664525u + 1013904223u;
            out[i] = hex[(seed >> 24) & 15];
        }
        out[32] = 0;
    }
    {
        char dir[1024];
        bq_home_join(dir, sizeof(dir), ".buraaq");
        buraaq_mkdir(dir);
        bq_home_join(dir, sizeof(dir), ".buraaq/dock");
        bur_mkdir_p(dir);
        buraaq_file_write(path, out);
    }
}

static void bq_http_reply(bq_hsock c, int status, const char *ctype, const char *body, size_t blen) {
    char hdr[256];
    int n;
    if (!body) {
        body = "";
        blen = 0;
    }
    n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
        status, status == 200 ? "OK" : status == 201 ? "Created" : status == 204 ? "No Content" : status == 401 ? "Unauthorized" : status == 404 ? "Not Found" : "Error",
        ctype ? ctype : "text/plain", (unsigned)blen);
    if (n > 0) send(c, hdr, (size_t)n, 0);
    if (blen) send(c, body, blen, 0);
}

static bq_hsock bq_listen_port(int port, int public_bind) {
    bq_hsock fd;
    struct sockaddr_in addr;
    int one = 1;
    if (!bq_hstart()) return BQ_HINV;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == BQ_HINV) return BQ_HINV;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = public_bind ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        bq_hclose(fd);
        return BQ_HINV;
    }
    if (listen(fd, 16) != 0) {
        bq_hclose(fd);
        return BQ_HINV;
    }
    return fd;
}

static int bq_http_read(bq_hsock c, char *hdr, size_t hcap, size_t *hlen, char **body, size_t *blen) {
    size_t n = 0;
    int r;
    char *sep;
    const char *cl;
    size_t need = 0;
    *body = NULL;
    *blen = 0;
    *hlen = 0;
    while (n + 1 < hcap) {
        r = recv(c, hdr + n, (int)(hcap - 1 - n), 0);
        if (r <= 0) break;
        n += (size_t)r;
        hdr[n] = 0;
        sep = strstr(hdr, "\r\n\r\n");
        if (sep) {
            *hlen = (size_t)(sep - hdr);
            cl = hdr;
            while (cl && *cl) {
                if ((cl[0] == 'C' || cl[0] == 'c') && strncmp(cl, "Content-Length:", 15) == 0) {
                    need = (size_t)strtoul(cl + 15, NULL, 10);
                    break;
                }
                if ((cl[0] == 'C' || cl[0] == 'c') && strncmp(cl, "content-length:", 15) == 0) {
                    need = (size_t)strtoul(cl + 15, NULL, 10);
                    break;
                }
                cl = strstr(cl, "\r\n");
                if (cl) cl += 2;
            }
            if (need > 64u * 1024u * 1024u) return 0;
            {
                size_t have = n - (*hlen + 4);
                char *buf = (char *)malloc(need + 1);
                if (!buf) return 0;
                if (have > need) have = need;
                if (have) memcpy(buf, sep + 4, have);
                while (have < need) {
                    r = recv(c, buf + have, (int)(need - have), 0);
                    if (r <= 0) break;
                    have += (size_t)r;
                }
                buf[have] = 0;
                *body = buf;
                *blen = have;
            }
            return 1;
        }
    }
    hdr[n] = 0;
    *hlen = n;
    return 1;
}

static int bq_nicmp(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + 32);
        if (ca != cb || ca == 0) return (int)ca - (int)cb;
    }
    return 0;
}

static const char *bq_hdr_auth(const char *hdr) {
    const char *p = hdr;
    while (p && *p) {
        if ((p[0] == 'A' || p[0] == 'a') && bq_nicmp(p, "Authorization:", 14) == 0) {
            p += 14;
            while (*p == ' ') p++;
            if (bq_nicmp(p, "Bearer ", 7) == 0) return p + 7;
            return p;
        }
        p = strstr(p, "\r\n");
        if (p) p += 2;
    }
    return "";
}

static void bq_hdr_line_end(char *s) {
    char *p = s;
    while (*p && *p != '\r' && *p != '\n' && *p != ' ') p++;
    *p = 0;
}

int32_t buraaq_dock_run(int32_t public_bind) {
    char token[128];
    char live[1024];
    bq_hsock fd;
    const char *once = getenv("BURAAQ_DOCK_ONCE");
    bq_dock_token(token, sizeof(token));
    bq_home_join(live, sizeof(live), ".buraaq/dock/live");
    bur_mkdir_p(live);
    fd = bq_listen_port(7422, public_bind);
    if (fd == BQ_HINV) {
        fprintf(stderr, "buraaq dock: bind :7422 failed\n");
        return 1;
    }
    fprintf(stderr, "buraaq dock: GET /v1/health on :7422\n");
    fflush(stderr);
    for (;;) {
        bq_hsock c = accept(fd, NULL, NULL);
        char hdr[8192];
        size_t hlen = 0;
        char *body = NULL;
        size_t blen = 0;
        char method[16];
        char path[512];
        const char *auth;
        int authed;
        if (c == BQ_HINV) {
            if (once && once[0] == '1') break;
            continue;
        }
        if (!bq_http_read(c, hdr, sizeof(hdr), &hlen, &body, &blen)) {
            bq_http_reply(c, 400, "text/plain", "bad request", 11);
            bq_hclose(c);
            free(body);
            if (once && once[0] == '1') break;
            continue;
        }
        method[0] = 0;
        path[0] = 0;
        sscanf(hdr, "%15s %511s", method, path);
        auth = bq_hdr_auth(hdr);
        {
            char abuf[128];
            size_t i = 0;
            while (auth[i] && auth[i] != '\r' && auth[i] != '\n' && i + 1 < sizeof(abuf)) {
                abuf[i] = auth[i];
                i++;
            }
            abuf[i] = 0;
            authed = bq_tok_eq(abuf, token);
        }
        if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/health") == 0) {
            bq_http_reply(c, 200, "application/json", "{\"ok\":true}", 11);
        } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/apps") == 0) {
            if (!authed) {
                bq_http_reply(c, 401, "text/plain", "unauthorized", 12);
            } else {
                bq_http_reply(c, 200, "application/json", "[]", 2);
            }
        } else if (strncmp(path, "/v1/apps/", 9) == 0) {
            char name[128];
            snprintf(name, sizeof(name), "%s", path + 9);
            bq_hdr_line_end(name);
            if (!bq_app_name_ok(name)) {
                bq_http_reply(c, 400, "text/plain", "bad name", 8);
            } else if (!authed) {
                bq_http_reply(c, 401, "text/plain", "unauthorized", 12);
            } else if (strcmp(method, "PUT") == 0) {
                char dest[1200];
                char dir[1100];
                snprintf(dir, sizeof(dir), "%s/%s", live, name);
                bur_mkdir_p(dir);
                snprintf(dest, sizeof(dest), "%s/%s/app.bur", live, name);
                {
                    FILE *fp = fopen(dest, "wb");
                    if (!fp) {
                        bq_http_reply(c, 500, "text/plain", "write failed", 12);
                    } else {
                        if (blen) fwrite(body, 1, blen, fp);
                        fclose(fp);
                        bq_http_reply(c, 201, "application/json", "{\"ok\":true}", 11);
                    }
                }
            } else if (strcmp(method, "DELETE") == 0) {
                char dest[1200];
                snprintf(dest, sizeof(dest), "%s/%s/app.bur", live, name);
                remove(dest);
                bq_http_reply(c, 204, "text/plain", "", 0);
            } else if (strcmp(method, "GET") == 0) {
                char dest[1200];
                snprintf(dest, sizeof(dest), "%s/%s/app.bur", live, name);
                if (buraaq_file_exists(dest)) {
                    bq_http_reply(c, 200, "application/json", "{\"ok\":true}", 11);
                } else {
                    bq_http_reply(c, 404, "text/plain", "not found", 9);
                }
            } else {
                bq_http_reply(c, 405, "text/plain", "method not allowed", 18);
            }
        } else {
            bq_http_reply(c, 404, "text/plain", "not found", 9);
        }
        free(body);
        bq_hclose(c);
        if (once && once[0] == '1') break;
    }
    bq_hclose(fd);
    return 0;
}

int32_t buraaq_index_run(const char *root) {
    bq_hsock fd;
    const char *once = getenv("BURAAQ_INDEX_ONCE");
    const char *base = root && root[0] ? root : "packages";
    fd = bq_listen_port(7423, 0);
    if (fd == BQ_HINV) {
        fprintf(stderr, "buraaq index: bind :7423 failed\n");
        return 1;
    }
    fprintf(stderr, "buraaq index: GET /index.json on :7423 from %s\n", base);
    fflush(stderr);
    for (;;) {
        bq_hsock c = accept(fd, NULL, NULL);
        char hdr[8192];
        size_t hlen = 0;
        char *body = NULL;
        size_t blen = 0;
        char method[16];
        char path[512];
        if (c == BQ_HINV) {
            if (once && once[0] == '1') break;
            continue;
        }
        if (!bq_http_read(c, hdr, sizeof(hdr), &hlen, &body, &blen)) {
            bq_http_reply(c, 400, "text/plain", "bad request", 11);
            bq_hclose(c);
            free(body);
            if (once && once[0] == '1') break;
            continue;
        }
        free(body);
        method[0] = 0;
        path[0] = 0;
        sscanf(hdr, "%15s %511s", method, path);
        if (strcmp(method, "GET") != 0) {
            bq_http_reply(c, 405, "text/plain", "method not allowed", 18);
        } else {
            const char *rel = path[0] == '/' ? path + 1 : path;
            char full[1200];
            char *file;
            if (!rel[0] || strcmp(rel, "index.json") == 0) rel = "index.json";
            if (strstr(rel, "..") || rel[0] == '/' || rel[0] == '\\') {
                bq_http_reply(c, 400, "text/plain", "bad path", 8);
            } else {
                snprintf(full, sizeof(full), "%s/%s", base, rel);
                file = buraaq_file_read(full);
                if (!file) {
                    bq_http_reply(c, 404, "text/plain", "not found", 9);
                } else {
                    const char *ct = strstr(rel, ".json") ? "application/json" : "text/plain";
                    bq_http_reply(c, 200, ct, file, strlen(file));
                    free(file);
                }
            }
        }
        bq_hclose(c);
        if (once && once[0] == '1') break;
    }
    bq_hclose(fd);
    return 0;
}
