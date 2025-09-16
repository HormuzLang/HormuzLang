/*
 *
 * ////////// Hormuz Programming Language \\\\\\\\\\
 * 
 * Persian C -> Raw C transpiler
 * Author: Max Base
 *
 * 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ----------------------------- Types ----------------------------- */
enum TokenType {
    TK_EOF,
    TK_IDENT,
    TK_NUMBER,
    TK_STRING,
    TK_KW,
    TK_OP,
};

struct Token {
    enum TokenType type;
    char *lexeme;
    int line;
};

struct TokenArray {
    struct Token *items;
    size_t count;
    size_t cap;
};

/* ----------------------------- Helpers ----------------------------- */
static char *strdup_s(const char *s) {
    if (!s) return NULL;
    size_t L = strlen(s);
    char *r = malloc(L + 1);
    if (!r) { perror("malloc"); exit(1); }
    memcpy(r, s, L + 1);
    return r;
}

static void token_array_init(struct TokenArray *ta) {
    ta->items = NULL; ta->count = 0; ta->cap = 0;
}
static void token_array_push(struct TokenArray *ta, struct Token t) {
    if (ta->count + 1 > ta->cap) {
        ta->cap = ta->cap ? ta->cap * 2 : 16;
        ta->items = realloc(ta->items, ta->cap * sizeof(*ta->items));
        if (!ta->items) { perror("realloc"); exit(1); }
    }
    ta->items[ta->count++] = t;
}
static void token_array_free(struct TokenArray *ta) {
    for (size_t i=0;i<ta->count;i++) free(ta->items[i].lexeme);
    free(ta->items); ta->items = NULL; ta->count = ta->cap = 0;
}

/* ----------------------------- Keyword / symbol map ----------------------------- */
struct Map { const char *p; const char *c; };

static struct Map keyword_map[] = {
    {"اگر", "if"},
    {"جاپ", "printf"},
    {"وگرنه", "else"},
    {"برای", "for"},
    {"درحالی که", "while"},
    {"اشاره", "void*"},
    {"تابع", "void"},
    {"بازگردان", "return"},
    {"صحیح", "int"},
    {"اعشاری", "double"},
    {"رشته", "char*"},
    {"و", "&&"},
    {"یا", "||"},
    {"برابر", "=="},
    {"نابرابر", "!="},
    {"کمتر", "<"},
    {"بزرگتر", ">"},
    {"افزایش", "++"},
    {NULL, NULL}
};

static const char *symbols[] = {
    "==", "!=", "++", "--", "&&", "||",
    "{","}","(",")",",",";","+","-","*","/","=","<",">","%","!", NULL
};

/* ----------------------------- Lexer ----------------------------- */
static const char *src = NULL;
static size_t pos = 0;
static int line_no = 1;

static void skip_space(void) {
    while (src[pos]) {
        if (src[pos] == '\n') { line_no++; pos++; }
        else if (isspace((unsigned char)src[pos])) pos++;
        else break;
    }
}

static int is_ident_start(unsigned char c) {
    return (isalpha(c) || c == '_' || (c & 0x80));
}

static char *read_identifier(void) {
    size_t start = pos;
    while (src[pos]) {
        unsigned char c = (unsigned char)src[pos];
        if (c & 0x80) { pos++; continue; }
        if (isalnum(c) || c == '_' ) { pos++; continue; }
        break;
    }
    size_t len = pos - start;
    char *s = malloc(len + 1);
    memcpy(s, src + start, len);
    s[len] = 0;
    return s;
}

static char *read_number(void) {
    size_t start = pos;
    while (isdigit((unsigned char)src[pos])) pos++;
    if (src[pos] == '.') { pos++; while (isdigit((unsigned char)src[pos])) pos++; }
    size_t len = pos - start;
    char *s = malloc(len + 1);
    memcpy(s, src + start, len); s[len] = 0;
    return s;
}

static char *read_string(void) {
    pos++;
    size_t start = pos;
    while (src[pos] && src[pos] != '"') {
        if (src[pos] == '\\' && src[pos+1]) pos += 2;
        else pos++;
    }
    size_t len = pos - start;
    char *s = malloc(len + 1);
    memcpy(s, src + start, len); s[len] = 0;
    if (src[pos] == '"') pos++;
    return s;
}

static int try_keyword_map_exact(const char *ident, const char **out_c) {
    for (int i=0; keyword_map[i].p; ++i) {
        if (strcmp(keyword_map[i].p, ident) == 0) { *out_c = keyword_map[i].c; return 1; }
    }
    return 0;
}

static void lex_all_to_array(const char *input, struct TokenArray *out) {
    src = input; pos = 0; line_no = 1;
    token_array_init(out);
    skip_space();
    while (src[pos]) {
        if (isspace((unsigned char)src[pos])) { skip_space(); continue; }

        if (isdigit((unsigned char)src[pos])) {
            char *num = read_number();
            struct Token t = { TK_NUMBER, num, line_no };
            token_array_push(out, t);
            continue;
        }

        if (src[pos] == '"') {
            char *s = read_string();
            struct Token t = { TK_STRING, s, line_no };
            token_array_push(out, t);
            continue;
        }

        if (is_ident_start((unsigned char)src[pos])) {
            char *id = read_identifier();
            const char *mapped = NULL;
            if (try_keyword_map_exact(id, &mapped)) {
                struct Token t = { TK_KW, strdup_s(mapped), line_no };
                token_array_push(out, t);
                free(id);
                continue;
            } else {
                struct Token t = { TK_IDENT, id, line_no };
                token_array_push(out, t);
                continue;
            }
        }

        int matched = 0;
        for (int i=0; symbols[i]; ++i) {
            size_t L = strlen(symbols[i]);
            if (strncmp(src + pos, symbols[i], L) == 0) {
                struct Token t = { TK_OP, strdup_s(symbols[i]), line_no };
                token_array_push(out, t);
                pos += L; matched = 1; break;
            }
        }
        if (matched) continue;

        if (strchr("{}(),;+-*/=%<>![]", src[pos])) {
            char tmp[2] = { src[pos], 0 };
            struct Token t = { TK_OP, strdup_s(tmp), line_no };
            token_array_push(out, t);
            pos++; continue;
        }

        pos++;
    }
    struct Token eof = { TK_EOF, strdup_s("<eof>"), line_no };
    token_array_push(out, eof);
}

/* ----------------------------- Generator ----------------------------- */
static void generate_raw_c(struct TokenArray *toks, FILE *out) {
    for (size_t i=0; i<toks->count; i++) {
        struct Token *t = &toks->items[i];
        if (t->type == TK_EOF) break;
        if (t->type == TK_STRING) {
            fprintf(out, "\"%s\"", t->lexeme);
        } else {
            fprintf(out, "%s", t->lexeme);
        }
        fprintf(out, " ");
    }
}

/* ----------------------------- Main ----------------------------- */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -i input -o output\n", argv[0]);
        return 1;
    }

    const char *infile = NULL, *outfile = NULL;
    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"-i")==0 && i+1<argc) infile = argv[++i];
        else if (strcmp(argv[i],"-o")==0 && i+1<argc) outfile = argv[++i];
    }

    if (!infile || !outfile) {
        fprintf(stderr, "Missing input/output\n");
        return 1;
    }

    FILE *f = fopen(infile,"rb");
    if (!f) { perror("fopen input"); return 1; }
    fseek(f,0,SEEK_END);
    long sz = ftell(f);
    fseek(f,0,SEEK_SET);
    char *buf = malloc(sz+1);
    fread(buf,1,sz,f);
    buf[sz]=0; fclose(f);

    struct TokenArray toks;
    lex_all_to_array(buf,&toks);

    FILE *fo = fopen(outfile,"wb");
    if (!fo) { perror("fopen output"); return 1; }
    generate_raw_c(&toks, fo);
    fclose(fo);

    token_array_free(&toks);
    free(buf);
    return 0;
}
