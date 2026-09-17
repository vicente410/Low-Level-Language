#include <stdio.h>
#include <ctype.h>
#define UTILS_IMPLEMENTATION
#include "utils.h"

// --- LEXER ---

typedef struct {
    String_View filename;
    size_t line;
    size_t col;
} Position;

String_View position_to_sv(Position pos) {
    String_Builder sb = {0};

    sb_appendf(&sb, SV_FMT":", SV_ARG(pos.filename));
    sb_appendf(&sb, "%zu:", pos.line);
    sb_appendf(&sb, "%zu:", pos.col);

    return sv_from_sb(sb);
}

typedef enum {
    TOKEN_EOF,
    TOKEN_ID,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LCURLY,
    TOKEN_RCURLY,
    TOKEN_INT_LIT,
    TOKEN_STRING_LIT,
} TokenKind;

typedef struct {
    TokenKind kind;
    union {
        String_View id;
        int int_lit;
        String_View string_lit;
    } as;
    Position pos;
} Token;

typedef struct {
    Token *data;
    size_t count;
    size_t capacity;
} Tokens;

typedef struct {
    FILE *fp;
    Position pos;
    char peeked_char;
    bool has_peeked_char;
    Token peeked_token;
    bool has_peeked_token;
} Lexer;

bool lexer_init(Lexer *lexer, char *filename) {
    lexer->fp = fopen(filename, "r");
    lexer->pos = (Position) {
        .filename = sv_from_cstr(filename),
        .line = 0,
        .col = 0,
    };
    lexer->has_peeked_char = false;
    lexer->has_peeked_token = false;

    return lexer->fp != NULL;
}

String_View token_to_sv(Token token) {
    String_Builder sb = {};

    switch (token.kind) {
    case TOKEN_EOF:        sb_appendf(&sb, "TOKEN_EOF"); break;
    case TOKEN_ID:         sb_appendf(&sb, "TOKEN_ID("SV_FMT")", SV_ARG(token.as.id)); break;
    case TOKEN_COLON:      sb_appendf(&sb, "TOKEN_COLON"); break;
    case TOKEN_SEMICOLON:  sb_appendf(&sb, "TOKEN_SEMICOLON"); break;
    case TOKEN_LPAREN:     sb_appendf(&sb, "TOKEN_LPAREN"); break;
    case TOKEN_RPAREN:     sb_appendf(&sb, "TOKEN_RPAREN"); break;
    case TOKEN_LCURLY:     sb_appendf(&sb, "TOKEN_LCURLY"); break;
    case TOKEN_RCURLY:     sb_appendf(&sb, "TOKEN_RCURLY"); break;
    case TOKEN_INT_LIT:    sb_appendf(&sb, "TOKEN_INT_LIT(%d)", token.as.int_lit); break;
    case TOKEN_STRING_LIT: sb_appendf(&sb, "TOKEN_STRING_LIT("SV_FMT")",
                                      SV_ARG(token.as.string_lit)); break;
    default:
        fprintf(stderr, "UNREACHABLE\n");
        exit(1);
    }

    return sv_from_sb(sb);
}

char next_char(Lexer *lexer) {
    char ch;

    if (lexer->has_peeked_char) {
        lexer->has_peeked_char = false;
        ch = lexer->peeked_char;
    } else {
        ch = fgetc(lexer->fp);
    }

    if (ch == '\n') {
        lexer->pos.line++;
        lexer->pos.col = 0;
    } else {
        lexer->pos.col++;
    }

    return ch;
}

char peek_char(Lexer *lexer) {
    if (!(lexer->has_peeked_char)) {
        lexer->peeked_char = next_char(lexer);
        lexer->has_peeked_char = true;
    }
    return lexer->peeked_char;
}

bool accept_char(Lexer *lexer, char ch) {
    if (peek_char(lexer) == ch) {
        next_char(lexer);
        return true;
    }

    return false;
}

Token read_id(Lexer *lexer) {
    Token token = {};
    String_Builder sb = {};

    token.pos = lexer->pos;
    token.kind = TOKEN_ID;

    while (!isspace(peek_char(lexer)) && !strchr(":;(){}", peek_char(lexer))) {
        da_push(&sb, next_char(lexer));
    }

    token.as.id = sv_from_sb(sb);
    return token;
}

Token read_int_lit(Lexer *lexer) {
    Token token = {};

    token.pos = lexer->pos;

    int int_lit = next_char(lexer) - '0';

    while(isdigit(peek_char(lexer))) {
        int_lit *= 10;
        int_lit += next_char(lexer) - '0';
    }

    token.kind = TOKEN_INT_LIT;
    token.as.int_lit = int_lit;

    return token;
}

Token read_string_lit(Lexer *lexer) {
    Token token = {};
    String_Builder sb = {};
    char ch;

    token.pos = lexer->pos;

    next_char(lexer);
    while ((ch = next_char(lexer)) != '"') {
        da_push(&sb, ch);
    }

    token.kind = TOKEN_STRING_LIT;
    token.as.string_lit = sv_from_sb(sb);

    return token;
}

Token read_symbol(Lexer *lexer) {
    Token token = {};
    char ch = next_char(lexer);

    token.pos = lexer->pos;

    switch (ch) {
    case ':': token.kind = TOKEN_COLON; break;
    case ';': token.kind = TOKEN_SEMICOLON; break;
    case '(': token.kind = TOKEN_LPAREN; break;
    case ')': token.kind = TOKEN_RPAREN; break;
    case '{': token.kind = TOKEN_LCURLY; break;
    case '}': token.kind = TOKEN_RCURLY; break;
    default:
        assert(false);
    }

    return token;
}

Token next_token(Lexer *lexer) {
    if (lexer->has_peeked_token) {
        lexer->has_peeked_token = false;
        return lexer->peeked_token;
    }

    while (peek_char(lexer) == ' ' || peek_char(lexer) == '\n' || peek_char(lexer) == '\t') {
        next_char(lexer);
    }

    if (feof(lexer->fp)) {
    	return (Token) { .kind = TOKEN_EOF };
    } else if (strchr(":;(){}", peek_char(lexer))) {
    	return read_symbol(lexer);
    } else if (isdigit(peek_char(lexer))) {
        return read_int_lit(lexer);
    } else if (peek_char(lexer) == '"') {
        return read_string_lit(lexer);
    } else {
        return read_id(lexer);
    }
}

Token peek_token(Lexer *lexer) {
    if (!(lexer->has_peeked_token)) {
        lexer->peeked_token = next_token(lexer);
        lexer->has_peeked_token = true;
    }
    return lexer->peeked_token;
}

void expect_token(Lexer *lexer, TokenKind kind) {
    if (next_token(lexer).kind != kind) {
        fprintf(stderr, "Error: Unexepected token");
        exit(1);
    }
}

bool accept_token(Lexer *lexer, TokenKind kind) {
    if (peek_token(lexer).kind == kind) {
        next_token(lexer);
        return true;
    }

    return false;
}

// --- PARSER ---

struct Term;

typedef struct {
    struct Term **data;
    size_t count;
    size_t capacity;
} Terms;

typedef enum {
    TERM_WORD,
    TERM_INT_LIT,
    TERM_STRING_LIT,
    TERM_QUOTATION,
} TermKind;

typedef struct Term {
    TermKind kind;
    union {
        String_View word;
        int int_lit;
        String_View string_lit;
        Terms quotation;
    } as;
} Term;

typedef struct {
    String_View name;
    Terms terms;
} Def;

typedef struct {
    Def **data;
    size_t count;
    size_t capacity;
} Program;

String_View term_to_sv(Term *term, size_t indent) {
    String_Builder sb = {};

    for (size_t i = 0; i < indent; i++) sb_appendf(&sb, "    ");

    switch (term->kind) {
    case TERM_WORD:       sb_appendf(&sb, "WORD("SV_FMT")", SV_ARG(term->as.word)); break;
    case TERM_INT_LIT:    sb_appendf(&sb, "INT(%d)", term->as.int_lit); break;
    case TERM_STRING_LIT: sb_appendf(&sb, "STRING("SV_FMT")", SV_ARG(term->as.string_lit)); break;
    case TERM_QUOTATION: {
        sb_appendf(&sb, "QUOTATION\n");

        for (size_t i = 0; i < term->as.quotation.count; i++) {
            String_View term_sv = term_to_sv(term->as.quotation.data[i], indent + 1);
            sb_appendf(&sb, SV_FMT, SV_ARG(term_sv));
            if (i < term->as.quotation.count - 1) sb_appendf(&sb, "\n");
        }
    } break;
    }

    return sv_from_sb(sb);
}

String_View def_to_sv(Def* def, size_t indent) {
    String_Builder sb = {};

    for (size_t i = 0; i < indent; i++) sb_appendf(&sb, "    ");

    sb_appendf(&sb, "DEF "SV_FMT"\n", SV_ARG(def->name));
    for (size_t i = 0; i < def->terms.count; i++) {
        String_View term_sv = term_to_sv(def->terms.data[i], indent + 1);
        sb_appendf(&sb, SV_FMT, SV_ARG(term_sv));
        if (i < def->terms.count - 1) sb_appendf(&sb, "\n");
    }

    return sv_from_sb(sb);
}

Term *parse_term(Lexer *lexer) {
    Term *result = calloc(1, sizeof(Term));
    Token token = next_token(lexer);

    if (token.kind == TOKEN_ID) {
        result->as.word = token.as.id;
        result->kind = TERM_WORD;
    } else if (token.kind == TOKEN_INT_LIT) {
        result->as.int_lit = token.as.int_lit;
        result->kind = TERM_INT_LIT;
    } else if (token.kind == TOKEN_STRING_LIT) {
        result->as.string_lit = token.as.string_lit;
        result->kind = TERM_STRING_LIT;
    } else if (token.kind == TOKEN_LCURLY) {
        result->kind = TERM_QUOTATION;

        while (!accept_token(lexer, TOKEN_RCURLY)) {
            da_push(&result->as.quotation, parse_term(lexer));
        }
    } else {
        String_View pos_sv = position_to_sv(token.pos);
        fprintf(stderr, SV_FMT" ERROR: expected term\n", SV_ARG(pos_sv));
        exit(1);
    }

    return result;
}

Def *parse_def(Lexer *lexer) {
    Def *result = calloc(1, sizeof(Def));
    expect_token(lexer, TOKEN_COLON);
    Token token = next_token(lexer);

    if (token.kind == TOKEN_ID) {
        result->name = token.as.id;
    } else {
        String_View pos_sv = position_to_sv(token.pos);
        fprintf(stderr, SV_FMT" ERROR: expected identifier\n", SV_ARG(pos_sv));
        exit(1);
    }

    while (!accept_token(lexer, TOKEN_SEMICOLON)) {
        da_push(&result->terms, parse_term(lexer));
    }

    return result;
}

Program *parse_program(Lexer *lexer) {
    Program *result = calloc(1, sizeof(Program));

    while (peek_token(lexer).kind != TOKEN_EOF) {
        da_push(result, parse_def(lexer));
    }

    return result;
}

// --- COMPILE ---

typedef struct {
    String_Builder data;
    String_Builder code;
} Output;

void compile_term(Term* term, String_Builder *code) {
    switch (term->kind) {
    case TERM_WORD:       sb_appendf(code, "    call "SV_FMT"\n", SV_ARG(term->as.word)); break;
    case TERM_INT_LIT:
        sb_appendf(code, "    mov qword [rbp], %d\n", term->as.int_lit);
        sb_appendf(code, "    add rbp, 8\n", term->as.int_lit);
        break;
    case TERM_STRING_LIT:
    case TERM_QUOTATION:
        fprintf(stderr, "TODO: implement");
        exit(1);
    }
}

void compile_def(Def *def, String_Builder *code) {
    sb_appendf(code, SV_FMT":\n", SV_ARG(def->name));

    for (size_t i = 0; i < def->terms.count; i++) {
        compile_term(def->terms.data[i], code);
    }

    sb_appendf(code, "    ret", SV_ARG(def->name));
}

String_View compile_program(Program *program) {
    String_Builder sb = {0};
    sb_appendf(&sb, "format ELF64 executable 3\n");
    sb_appendf(&sb, "\n");
    sb_appendf(&sb, "segment readable executable\n");
    sb_appendf(&sb, "entry _start\n");
    sb_appendf(&sb, "_start:\n");
    sb_appendf(&sb, "    mov rbp, data_stack\n");
    sb_appendf(&sb, "    call main\n");
    sb_appendf(&sb, "    mov rax, 60\n");
    sb_appendf(&sb, "    mov rdi, [rbp - 8]\n");
    sb_appendf(&sb, "    syscall\n");
    sb_appendf(&sb, "\n");

    for (size_t i = 0; i < program->count; i++) {
        compile_def(program->data[i], &sb);
    }

    sb_appendf(&sb, "\n");
    sb_appendf(&sb, "segment readable writeable\n");
    sb_appendf(&sb, "data_stack rd 8192\n");

    return sv_from_sb(sb);
}

// --- MAIN ---

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        exit(1);
    }

    char* program_name = argv[1];

    Lexer lexer = {};
    lexer_init(&lexer, program_name);
    Program *program = parse_program(&lexer);
    String_View assembly = compile_program(program);
    printf(SV_FMT, SV_ARG(assembly));

    return 0;
}
