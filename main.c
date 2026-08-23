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

typedef enum {
    TOKEN_EOF,
    TOKEN_ID,
    TOKEN_COLON,
    TOKEN_EQUAL,
    TOKEN_COMMA,
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_CURLY,
    TOKEN_CLOSE_CURLY,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FN,
    TOKEN_STRUCT,
    TOKEN_UNION,
    TOKEN_INT_LIT,
    TOKEN_STRING_LIT,
    TOKEN_SEMICOLON,
    TOKEN_OP,
} TokenKind;

typedef struct {
    TokenKind kind;
    union {
        String_View id;
        int int_lit;
        String_View string_lit;
        String_View op;
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
    case TOKEN_EOF:         sb_appendf(&sb, "TOKEN_EOF"); break;
    case TOKEN_ID:          sb_appendf(&sb, "TOKEN_ID("SV_FMT")", SV_ARG(token.as.id)); break;
    case TOKEN_COLON:       sb_appendf(&sb, "TOKEN_COLON"); break;
    case TOKEN_COMMA:       sb_appendf(&sb, "TOKEN_COMMA"); break;
    case TOKEN_OPEN_PAREN:  sb_appendf(&sb, "TOKEN_OPEN_PAREN"); break;
    case TOKEN_CLOSE_PAREN: sb_appendf(&sb, "TOKEN_CLOSE_PAREN"); break;
    case TOKEN_OPEN_CURLY:  sb_appendf(&sb, "TOKEN_OPEN_CURLY"); break;
    case TOKEN_CLOSE_CURLY: sb_appendf(&sb, "TOKEN_CLOSE_CURLY"); break;
    case TOKEN_RETURN:      sb_appendf(&sb, "TOKEN_RETURN"); break;
    case TOKEN_IF:          sb_appendf(&sb, "TOKEN_IF"); break;
    case TOKEN_ELSE:        sb_appendf(&sb, "TOKEN_ELSE"); break;
    case TOKEN_FN:          sb_appendf(&sb, "TOKEN_FN"); break;
    case TOKEN_STRUCT:      sb_appendf(&sb, "TOKEN_STRUCT"); break;
    case TOKEN_UNION:       sb_appendf(&sb, "TOKEN_UNION"); break;
    case TOKEN_INT_LIT:     sb_appendf(&sb, "TOKEN_INT_LIT(%d)", token.as.int_lit); break;
    case TOKEN_STRING_LIT:  sb_appendf(&sb, "TOKEN_STRING_LIT("SV_FMT")",
                                       SV_ARG(token.as.string_lit)); break;
    case TOKEN_SEMICOLON:   sb_appendf(&sb, "TOKEN_SEMICOLON"); break;
    case TOKEN_OP:          sb_appendf(&sb, "TOKEN_OP("SV_FMT")", SV_ARG(token.as.op)); break;
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

Token read_id_or_keyword(Lexer *lexer) {
    Token token = {};
    String_Builder sb = {};
    
    token.pos = lexer->pos;

    while (isalnum(peek_char(lexer))) {
        da_push(&sb, next_char(lexer));
    }

    if (sv_eq_cstr(sv_from_sb(sb), "return")) {
        token.kind = TOKEN_RETURN;
    } else if (sv_eq_cstr(sv_from_sb(sb), "if")) {
        token.kind = TOKEN_IF;
    } else if (sv_eq_cstr(sv_from_sb(sb), "else")) {
        token.kind = TOKEN_ELSE;
    } else if (sv_eq_cstr(sv_from_sb(sb), "fn")) {
        token.kind = TOKEN_FN;
    } else if (sv_eq_cstr(sv_from_sb(sb), "struct")) {
        token.kind = TOKEN_STRUCT;
    } else if (sv_eq_cstr(sv_from_sb(sb), "union")) {
        token.kind = TOKEN_UNION;
    } else {
        token.kind = TOKEN_ID;
        token.as.id = sv_from_sb(sb);
    }
    
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
    case '=': {
        if (accept_char(lexer, '=')) {
            token.kind = TOKEN_OP;
            token.as.op = sv_from_cstr("==");
        } else {
            token.kind = TOKEN_EQUAL;
        }
    } break;
    case ',': token.kind = TOKEN_COMMA; break;
    case '(': token.kind = TOKEN_OPEN_PAREN; break;
    case ')': token.kind = TOKEN_CLOSE_PAREN; break;
    case '{': token.kind = TOKEN_OPEN_CURLY; break;
    case '}': token.kind = TOKEN_CLOSE_CURLY; break;
    case ';': token.kind = TOKEN_SEMICOLON; break;
    default:
        token.kind = TOKEN_OP;
        String_Builder sb = {};
        da_push(&sb, ch);
        while (strchr("+-*/!=<>", peek_char(lexer)) != NULL) {
            da_push(&sb, next_char(lexer));
        }
        token.as.op = sv_from_sb(sb);
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
    } else if (isalpha(peek_char(lexer))) {
        return read_id_or_keyword(lexer);
    } else if (isdigit(peek_char(lexer))) {
        return read_int_lit(lexer);
    } else if (peek_char(lexer) == '"') {
        return read_string_lit(lexer);
    } else {
    	return read_symbol(lexer);
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

struct Ast;

typedef struct {
    struct Ast **data;
    size_t count;
    size_t capacity;
} Asts;

typedef enum {
    AST_ID,
    AST_INT_LIT,
    AST_STRING_LIT,
    AST_CALL,
    AST_OP,
    AST_BLOCK,
    AST_IFTE,
} AstKind;

typedef struct {
    String_View op;
    struct Ast *lhs;
    struct Ast *rhs;
} Op;

typedef struct {
    struct Ast *fn;
    Asts args;
} Call;

typedef struct {
    struct Ast *cond;
    struct Ast *then_branch;
    struct Ast *else_branch;
} Ifte;

typedef struct Ast {
    AstKind kind;
    union {
        String_View id;
        int int_lit;
        String_View string_lit;
        Op op;
        Call call;
        Asts block;
        Ifte ifte;
    } as;
} Ast;

String_View ast_to_sv(Ast *ast, size_t indent) {
    String_Builder sb = {};
    
    for (size_t i = 0; i < indent; i++) sb_appendf(&sb, "    ");
    
    switch (ast->kind) {
    case AST_ID:           sb_appendf(&sb, "ID("SV_FMT")", SV_ARG(ast->as.id)); break;
    case AST_INT_LIT:      sb_appendf(&sb, "INT(%d)", ast->as.int_lit); break;
    case AST_STRING_LIT:   sb_appendf(&sb, "STRING("SV_FMT")", SV_ARG(ast->as.string_lit)); break;
    case AST_OP: {
        String_View lhs_sv = ast_to_sv(ast->as.op.lhs, indent + 1);
        String_View rhs_sv = ast_to_sv(ast->as.op.rhs, indent + 1);
        sb_appendf(&sb, "OP("SV_FMT")\n", SV_ARG(ast->as.op.op));
        sb_appendf(&sb, SV_FMT"\n", SV_ARG(lhs_sv));
        sb_appendf(&sb, SV_FMT, SV_ARG(rhs_sv));
    } break;
    case AST_CALL: {
        String_View fn_sv = ast_to_sv(ast->as.call.fn, indent + 1);
        sb_appendf(&sb, "CALL\n");
        sb_appendf(&sb, SV_FMT, SV_ARG(fn_sv));
        
        for (size_t i = 0; i < ast->as.call.args.count; i++) {
            String_View arg_sv = ast_to_sv(ast->as.call.args.data[i], indent + 1);
            sb_appendf(&sb, "\n"SV_FMT, SV_ARG(arg_sv));
        }
    } break;
    case AST_BLOCK: {
        sb_appendf(&sb, "BLOCK\n");
        
        for (size_t i = 0; i < ast->as.block.count; i++) {
            String_View expr_sv = ast_to_sv(ast->as.block.data[i], indent + 1);
            sb_appendf(&sb, SV_FMT, SV_ARG(expr_sv));
            if (i < ast->as.block.count - 1) sb_appendf(&sb, "\n");
        }
    } break;
    case AST_IFTE: {
        String_View cond_sv = ast_to_sv(ast->as.ifte.cond, indent + 1);
        String_View then_sv = ast_to_sv(ast->as.ifte.then_branch, indent + 1);
        sb_appendf(&sb, "IFTE\n");
        sb_appendf(&sb, SV_FMT"\n", SV_ARG(cond_sv));
        sb_appendf(&sb, SV_FMT"\n", SV_ARG(then_sv));
        
        if (ast->as.ifte.else_branch != NULL) {
            String_View else_sv = ast_to_sv(ast->as.ifte.else_branch, indent + 1);
            sb_appendf(&sb, SV_FMT, SV_ARG(else_sv));
        }
    } break;
    }
    
    return sv_from_sb(sb);
}

Ast *parse_expr(Lexer *lexer, size_t precedence);

Ast *parse_factor(Lexer *lexer) {
    Ast *result;
    Token token = next_token(lexer);
    
    if (token.kind == TOKEN_ID) {
        result = malloc(sizeof(Ast));
        result->kind = AST_ID;
        result->as.id = token.as.id;
    } else if (token.kind == TOKEN_INT_LIT) {
        result = malloc(sizeof(Ast));
        result->kind = AST_INT_LIT;
        result->as.int_lit = token.as.int_lit;
    } else if (token.kind == TOKEN_STRING_LIT) {
        result = malloc(sizeof(Ast));
        result->kind = AST_STRING_LIT;
        result->as.string_lit = token.as.string_lit;
    } else if (token.kind == TOKEN_OPEN_PAREN) {
        result = parse_expr(lexer, 0);
        expect_token(lexer, TOKEN_CLOSE_PAREN);
    } else if (token.kind == TOKEN_OPEN_CURLY) {
        result = malloc(sizeof(Ast));
        result->kind = AST_BLOCK;
        
        while (!accept_token(lexer, TOKEN_CLOSE_CURLY)) {
            da_push(&result->as.block, parse_expr(lexer, 0));
            if (peek_token(lexer).kind != TOKEN_CLOSE_CURLY) {
                expect_token(lexer, TOKEN_SEMICOLON);
            }
        }
    } else if (token.kind == TOKEN_IF) {
        result = malloc(sizeof(Ast));
        result->kind = AST_IFTE;
        result->as.ifte.cond = parse_expr(lexer, 0);
        result->as.ifte.then_branch = parse_expr(lexer, 0);
        if (accept_token(lexer, TOKEN_ELSE)) {
            result->as.ifte.else_branch = parse_expr(lexer, 0);
        }
    } else {
        fprintf(stderr, "Error: syntax error\n");
        exit(1);
    }
    
    if (accept_token(lexer, TOKEN_OPEN_PAREN)) {
        Ast *new_result = malloc(sizeof(Ast));
        new_result->kind = AST_CALL;
        new_result->as.call.fn = result;
        result = new_result;
        
        while (!accept_token(lexer, TOKEN_CLOSE_PAREN)) {
            da_push(&result->as.call.args, parse_expr(lexer, 0));
            if (peek_token(lexer).kind != TOKEN_CLOSE_PAREN) {
                expect_token(lexer, TOKEN_COMMA);
            }
        }
    }

    return result;
}

Ast *parse_expr(Lexer *lexer, size_t precedence) {
    Ast *result = precedence == 3 ? parse_factor(lexer)
                                  : parse_expr(lexer, precedence + 1);
    
    while (peek_token(lexer).kind == TOKEN_OP && (
        (precedence == 2 && (
            sv_eq_cstr(peek_token(lexer).as.op, "*") ||
            sv_eq_cstr(peek_token(lexer).as.op, "/"))) ||
        (precedence == 1 && (
            sv_eq_cstr(peek_token(lexer).as.op, "+") ||
            sv_eq_cstr(peek_token(lexer).as.op, "-"))) ||
        (precedence == 0 && (
            sv_eq_cstr(peek_token(lexer).as.op, "<") ||
            sv_eq_cstr(peek_token(lexer).as.op, ">") ||
            sv_eq_cstr(peek_token(lexer).as.op, "<=") ||
            sv_eq_cstr(peek_token(lexer).as.op, ">=") ||
            sv_eq_cstr(peek_token(lexer).as.op, "==") ||
            sv_eq_cstr(peek_token(lexer).as.op, "!=")))
    )) {
        Ast *new_result = malloc(sizeof(Ast));
        new_result->kind = AST_OP;
        new_result->as.op.lhs = result;
        new_result->as.op.op = next_token(lexer).as.op;
        new_result->as.op.rhs = parse_expr(lexer, precedence + 1);
        result = new_result;
    }

    return result;
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
    Ast *ast = parse_expr(&lexer, 0);
    
    String_View ast_sv = ast_to_sv(ast, 0);
    printf(SV_FMT"\n", SV_ARG(ast_sv));
    
    /*for (size_t i = 0; i < asts.count; i++) {
        String_View ast_sv = ast_to_sv(asts.data[i]);
        printf(SV_FMT"\n", SV_ARG(ast_sv));
    }*/

    /*while (true) {
        Token token = next_token(&lexer);
        if (token.kind == TOKEN_EOF) break;
        String_View token_sv = token_to_sv(token);
        printf(SV_FMT"\n", SV_ARG(token_sv));
    }*/
    //Stmt stmt = parse_stmt(&lexer);
    //String_View stmt_sv = stmt_to_sv(stmt);
    //printf(SV_FMT"\n", SV_ARG(stmt_sv));

    return 0;
}
