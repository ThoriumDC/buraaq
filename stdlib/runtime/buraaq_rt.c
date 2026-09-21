// Buraaq minimal native runtime — linked with all generated executables.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>

/* Length of a `text`, remembering the strings most recently measured.

   `byte` and `slice` are called once per character of their input and both need
   the length, so walking the string to measure it made reading a file cost the
   square of its size: a compiler scanning a 30KB source re-measured those 30KB
   thirty thousand times.

   One slot is not enough. A scan interleaves reads of the source with length
   queries on short strings, and with a single slot each of those evicts the
   source so the next read re-measures it. A few slots keep the source resident
   alongside whatever else is in play.

   The slot given up is the one holding the shortest string, because the cost of
   a miss is the length of what was dropped. Round-robin eventually evicts the
   source, and re-measuring it once per statement is the same quadratic again
   with a smaller constant.

   This lives here rather than beside the text functions in buraaq_std.c because
   this file is linked into every binary, including the bootstrap builds that
   take the core runtime on its own.

   It is only correct while a remembered pointer still refers to the string it
   was measured from. A release hands the address back and a later allocation
   could land there with a different length, so every release in the runtime
   goes through bq_free or bq_realloc below. Any new release site must too. */
#define BQ_LEN_SLOTS 8
static const char *g_len_ptr[BQ_LEN_SLOTS];
static size_t g_len_val[BQ_LEN_SLOTS];

void buraaq_forget_length(void) {
    for (int i = 0; i < BQ_LEN_SLOTS; i++) {
        g_len_ptr[i] = NULL;
        g_len_val[i] = 0;
    }
}

size_t buraaq_length_of(const char *s) {
    if (!s) return 0;
    for (int i = 0; i < BQ_LEN_SLOTS; i++) {
        if (g_len_ptr[i] == s) return g_len_val[i];
    }
    size_t n = strlen(s);
    int weakest = 0;
    for (int i = 0; i < BQ_LEN_SLOTS; i++) {
        if (!g_len_ptr[i]) {
            weakest = i;
            break;
        }
        if (g_len_val[i] < g_len_val[weakest]) weakest = i;
    }
    if (g_len_ptr[weakest] == NULL || g_len_val[weakest] < n) {
        g_len_ptr[weakest] = s;
        g_len_val[weakest] = n;
    }
    return n;
}

static void bq_free(void *p) {
    buraaq_forget_length();
    free(p);
}

static void *bq_realloc(void *p, size_t n) {
    buraaq_forget_length();
    return realloc(p, n);
}

void buraaq_print_i32(int32_t n) { printf("%" PRId32, n); }
void buraaq_print_i32_ln(int32_t n) { printf("%" PRId32 "\n", n); }

void buraaq_print_i64(int64_t n) { printf("%" PRId64, n); }
void buraaq_print_i64_ln(int64_t n) { printf("%" PRId64 "\n", n); }

void buraaq_print_f64(double n) { printf("%g", n); }
void buraaq_print_f64_ln(double n) { printf("%g\n", n); }

void buraaq_print_bool(int b) { printf("%s", b ? "true" : "false"); }
void buraaq_print_bool_ln(int b) { printf("%s\n", b ? "true" : "false"); }

/* A `text` can hold NULL when an extern C call fails; treat it as empty so a
   failed call surfaces as blank output instead of a crash in the printer. */
void buraaq_print_str(const char *s) { fputs(s ? s : "", stdout); }
void buraaq_print_str_ln(const char *s) { puts(s ? s : ""); }

/* Diagnostics go to stderr so a compiler can keep writing its real output to
   stdout, and exit lets it stop at the first thing it cannot handle. */
void buraaq_eprint_str_ln(const char *s) {
    fputs(s ? s : "", stderr);
    fputc('\n', stderr);
}

void buraaq_exit(int32_t code) { exit(code); }

static char *buraaq_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *buraaq_read_line(void) {
    char buf[4096];
    fflush(stdout);
    fflush(stderr);
    if (!fgets(buf, (int)sizeof(buf), stdin)) {
        return buraaq_dup(":quit");
    }
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        n = n - 1;
        buf[n] = 0;
    }
    return buraaq_dup(buf);
}

char *buraaq_i32_to_text(int32_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId32, n);
    return buraaq_dup(buf);
}

char *buraaq_i64_to_text(int64_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, n);
    return buraaq_dup(buf);
}

char *buraaq_f64_to_text(double n) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", n);
    return buraaq_dup(buf);
}

char *buraaq_bool_to_text(int b) { return buraaq_dup(b ? "true" : "false"); }

void *buraaq_alloc(int64_t bytes) {
    if (bytes <= 0) return NULL;
    return malloc((size_t)bytes);
}

void buraaq_free(void *p) { bq_free(p); }

/* --- string set --------------------------------------------------------
   The bootstrap compiler tracked which locals it had declared in a
   comma-separated string, testing membership by scanning it and adding by
   rebuilding it. Both are linear in the number of locals, so declaring n of them
   cost O(n^2) and a function with a thousand locals dominated the whole compile.

   Chained buckets, FNV-1a, doubling when it fills. */

typedef struct BqSetNode {
    struct BqSetNode *next;
    char *key;
} BqSetNode;

typedef struct {
    BqSetNode **buckets;
    int32_t nbuckets;
    int32_t count;
} BqSet;

static uint64_t bq_hash(const char *s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 0x100000001b3ULL;
    }
    return h;
}

void *buraaq_set_new(void) {
    BqSet *s = (BqSet *)calloc(1, sizeof(BqSet));
    if (!s) return NULL;
    s->nbuckets = 64;
    s->buckets = (BqSetNode **)calloc((size_t)s->nbuckets, sizeof(BqSetNode *));
    if (!s->buckets) {
        bq_free(s);
        return NULL;
    }
    return s;
}

static void set_rehash(BqSet *s) {
    int32_t n = s->nbuckets * 2;
    BqSetNode **b = (BqSetNode **)calloc((size_t)n, sizeof(BqSetNode *));
    if (!b) return;
    for (int32_t i = 0; i < s->nbuckets; i++) {
        BqSetNode *node = s->buckets[i];
        while (node) {
            BqSetNode *next = node->next;
            uint64_t idx = bq_hash(node->key) & (uint64_t)(n - 1);
            node->next = b[idx];
            b[idx] = node;
            node = next;
        }
    }
    bq_free(s->buckets);
    s->buckets = b;
    s->nbuckets = n;
}

int32_t buraaq_set_has(void *h, const char *key) {
    BqSet *s = (BqSet *)h;
    if (!s || !key) return 0;
    uint64_t idx = bq_hash(key) & (uint64_t)(s->nbuckets - 1);
    for (BqSetNode *n = s->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) return 1;
    }
    return 0;
}

/* 1 when the key was added, 0 when it was already present. */
int32_t buraaq_set_add(void *h, const char *key) {
    BqSet *s = (BqSet *)h;
    if (!s || !key) return 0;
    if (buraaq_set_has(h, key)) return 0;
    if (s->count >= s->nbuckets) set_rehash(s);
    uint64_t idx = bq_hash(key) & (uint64_t)(s->nbuckets - 1);
    BqSetNode *n = (BqSetNode *)malloc(sizeof(BqSetNode));
    if (!n) return 0;
    n->key = buraaq_dup(key);
    n->next = s->buckets[idx];
    s->buckets[idx] = n;
    s->count++;
    return 1;
}

/* Add every comma-separated entry, skipping empties. Lets a caller seed a set
   from one of the compiler's existing comma bags. */
void buraaq_set_add_csv(void *h, const char *csv) {
    if (!h || !csv) return;
    const char *start = csv;
    for (;;) {
        const char *comma = strchr(start, ',');
        size_t len = comma ? (size_t)(comma - start) : strlen(start);
        if (len > 0) {
            char *item = (char *)malloc(len + 1);
            if (item) {
                memcpy(item, start, len);
                item[len] = '\0';
                buraaq_set_add(h, item);
                bq_free(item);
            }
        }
        if (!comma) break;
        start = comma + 1;
    }
}

int32_t buraaq_set_len(void *h) {
    BqSet *s = (BqSet *)h;
    return s ? s->count : 0;
}

void buraaq_set_free(void *h) {
    BqSet *s = (BqSet *)h;
    if (!s) return;
    for (int32_t i = 0; i < s->nbuckets; i++) {
        BqSetNode *n = s->buckets[i];
        while (n) {
            BqSetNode *next = n->next;
            bq_free(n->key);
            bq_free(n);
            n = next;
        }
    }
    bq_free(s->buckets);
    bq_free(s);
}

/* A one-character string from a byte value. The bootstrap compiler needs to emit
   `{`, `}` and `"`, none of which it can write as a literal: a brace opens an
   interpolation and a quote ends the string. It used to search its own source
   for each character, which is a whole-file scan per function. */
char *buraaq_char(int32_t b) {
    char buf[2];
    buf[0] = (char)b;
    buf[1] = '\0';
    return buraaq_dup(buf);
}

/* --- growable vector ---------------------------------------------------
   Symbol tables, block lists and instruction buffers all need a sequence that
   grows. The bootstrap compiler has been faking them with comma-separated text
   and linear scans, which is why several of its lookups are quadratic.

   One slot holds either an i32 or a pointer, so a vector can carry ints or
   text. The element type is not tracked: reading an int slot as text would
   misinterpret it, exactly as it would in C. Callers know which they pushed. */

typedef struct {
    int32_t len;
    int32_t cap;
    void **slots;
} BqVec;

void *buraaq_vec_new(void) {
    BqVec *v = (BqVec *)calloc(1, sizeof(BqVec));
    return v;
}

int32_t buraaq_vec_len(void *h) {
    BqVec *v = (BqVec *)h;
    return v ? v->len : 0;
}

/* Returns 0 on success, -1 if it could not grow. */
static int vec_grow(BqVec *v) {
    if (v->len < v->cap) return 0;
    int32_t cap = v->cap ? v->cap * 2 : 8;
    void **slots = (void **)bq_realloc(v->slots, (size_t)cap * sizeof(void *));
    if (!slots) return -1;
    v->slots = slots;
    v->cap = cap;
    return 0;
}

void buraaq_vec_push_int(void *h, int32_t x) {
    BqVec *v = (BqVec *)h;
    if (!v || vec_grow(v) != 0) return;
    v->slots[v->len++] = (void *)(intptr_t)x;
}

void buraaq_vec_push_text(void *h, const char *s) {
    BqVec *v = (BqVec *)h;
    if (!v || vec_grow(v) != 0) return;
    v->slots[v->len++] = buraaq_dup(s);
}

/* Out of range reads 0 / "" rather than trapping: the bootstrap compiler has no
   way to raise, and a zero is easier to track down than a corrupted read. */
int32_t buraaq_vec_get_int(void *h, int32_t i) {
    BqVec *v = (BqVec *)h;
    if (!v || i < 0 || i >= v->len) return 0;
    return (int32_t)(intptr_t)v->slots[i];
}

char *buraaq_vec_get_text(void *h, int32_t i) {
    BqVec *v = (BqVec *)h;
    if (!v || i < 0 || i >= v->len) return buraaq_dup("");
    return (char *)v->slots[i];
}

void buraaq_vec_set_int(void *h, int32_t i, int32_t x) {
    BqVec *v = (BqVec *)h;
    if (!v || i < 0 || i >= v->len) return;
    v->slots[i] = (void *)(intptr_t)x;
}

void buraaq_vec_set_text(void *h, int32_t i, const char *s) {
    BqVec *v = (BqVec *)h;
    if (!v || i < 0 || i >= v->len) return;
    v->slots[i] = buraaq_dup(s);
}

void buraaq_vec_free(void *h) {
    BqVec *v = (BqVec *)h;
    if (!v) return;
    bq_free(v->slots);
    bq_free(v);
}

/* A struct instance for the bootstrap compiler: `slots` eight-byte cells, one
   per field, wide enough for either an i32 or a pointer. Zeroed, so a field that
   is never assigned reads as 0 or NULL rather than as garbage. Takes i32
   because that is the only integer width the bootstrap compiler can express. */
void *buraaq_obj_new(int32_t slots) {
    if (slots <= 0) return NULL;
    return calloc((size_t)slots, 8);
}

char *buraaq_async_io_read_line(void) {
    return buraaq_read_line();
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((weak))
#endif
void buraaq_rt_set_args(int argc, char **argv) {
    (void)argc;
    (void)argv;
}
