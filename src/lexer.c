#include "preforth.h"

static bool is_ident_char(char c) {
    if (c == '\0' || isspace((unsigned char)c)) return false;
    if (c == ',' || c == '(' || c == ')' || c == ';') return false;
    return true;
}

static bool parse_number(const char *text, int64_t *out_int, double *out_float, bool *out_is_float) {
    size_t len = strlen(text);
    if (len == 0) return false;

    // Sole '+' or '-' is an operator, not a number
    if (len == 1 && (text[0] == '+' || text[0] == '-')) return false;

    // Check for hex: 0x or -0x
    size_t idx = 0;
    bool neg = false;
    if (text[idx] == '-') {
        neg = true;
        idx++;
    } else if (text[idx] == '+') {
        idx++;
    }

    if (idx + 2 <= len && text[idx] == '0' && (text[idx + 1] == 'x' || text[idx + 1] == 'X')) {
        char *endptr = NULL;
        unsigned long long uval = strtoull(text + idx, &endptr, 16);
        if (endptr && *endptr == '\0') {
            int64_t ival = neg ? -(int64_t)uval : (int64_t)uval;
            *out_int = ival;
            *out_is_float = false;
            return true;
        }
        return false;
    }

    // Check for floating point
    bool has_dot = false;
    bool has_exp = false;
    for (size_t i = idx; i < len; i++) {
        if (text[i] == '.') has_dot = true;
        if (text[i] == 'e' || text[i] == 'E') has_exp = true;
    }

    if (has_dot || has_exp) {
        char *endptr = NULL;
        double dval = strtod(text, &endptr);
        if (endptr && *endptr == '\0') {
            *out_float = dval;
            *out_is_float = true;
            return true;
        }
        return false;
    }

    // Base 10 integer
    char *endptr = NULL;
    long long ival = strtoll(text, &endptr, 10);
    if (endptr && *endptr == '\0') {
        *out_int = (int64_t)ival;
        *out_is_float = false;
        return true;
    }

    return false;
}

void token_free(Token *tok) {
    if (tok && tok->text) {
        free(tok->text);
        tok->text = NULL;
    }
}

void token_array_free(TokenArray *ta) {
    if (!ta) return;
    for (size_t i = 0; i < ta->count; i++) {
        token_free(&ta->tokens[i]);
    }
    free(ta->tokens);
    free(ta);
}

static void token_array_push(TokenArray *ta, Token tok) {
    if (ta->count >= ta->capacity) {
        ta->capacity = (ta->capacity == 0) ? 64 : ta->capacity * 2;
        ta->tokens = (Token *)realloc(ta->tokens, ta->capacity * sizeof(Token));
    }
    ta->tokens[ta->count++] = tok;
}

TokenArray *lexer_tokenize(const char *source) {
    TokenArray *ta = (TokenArray *)calloc(1, sizeof(TokenArray));
    int line = 1;
    int col = 1;
    const char *p = source;
    bool line_has_tokens = false;

    while (*p) {
        // Skip horizontal whitespace
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            col++;
            p++;
            continue;
        }

        // Handle line comment starting with '#'
        if (*p == '#') {
            while (*p && *p != '\n') {
                p++;
                col++;
            }
            continue;
        }

        // Handle newline: emit TOK_NL if line had tokens
        if (*p == '\n') {
            if (line_has_tokens) {
                Token tok;
                memset(&tok, 0, sizeof(tok));
                tok.type = TOK_NL;
                tok.text = strdup("<NL>");
                tok.line = line;
                tok.col = col;
                token_array_push(ta, tok);
                line_has_tokens = false;
            }
            line++;
            col = 1;
            p++;
            continue;
        }

        // Comma is an inline statement separator equivalent to <NL>
        if (*p == ',') {
            Token tok;
            memset(&tok, 0, sizeof(tok));
            tok.type = TOK_COMMA;
            tok.text = strdup(",");
            tok.line = line;
            tok.col = col;
            token_array_push(ta, tok);
            col++;
            p++;
            // A comma closes a statement, line can have more tokens
            continue;
        }

        // Semicolon ';'
        if (*p == ';') {
            Token tok;
            memset(&tok, 0, sizeof(tok));
            tok.type = TOK_SEMICOLON;
            tok.text = strdup(";");
            tok.line = line;
            tok.col = col;
            token_array_push(ta, tok);
            line_has_tokens = true;
            col++;
            p++;
            continue;
        }

        // Parentheses for FFI declarations
        if (*p == '(') {
            Token tok;
            memset(&tok, 0, sizeof(tok));
            tok.type = TOK_LPAREN;
            tok.text = strdup("(");
            tok.line = line;
            tok.col = col;
            token_array_push(ta, tok);
            line_has_tokens = true;
            col++;
            p++;
            continue;
        }
        if (*p == ')') {
            Token tok;
            memset(&tok, 0, sizeof(tok));
            tok.type = TOK_RPAREN;
            tok.text = strdup(")");
            tok.line = line;
            tok.col = col;
            token_array_push(ta, tok);
            line_has_tokens = true;
            col++;
            p++;
            continue;
        }

        // Double-quoted string literal
        if (*p == '"') {
            int start_col = col;
            p++; // skip opening quote
            col++;

            size_t str_cap = 64;
            size_t str_len = 0;
            char *str_buf = (char *)malloc(str_cap);

            while (*p && *p != '"') {
                char ch = *p++;
                col++;
                if (ch == '\\' && *p) {
                    char esc = *p++;
                    col++;
                    switch (esc) {
                        case 'n': ch = '\n'; break;
                        case 't': ch = '\t'; break;
                        case 'r': ch = '\r'; break;
                        case '\\': ch = '\\'; break;
                        case '"': ch = '"'; break;
                        case '0': ch = '\0'; break;
                        default: ch = esc; break;
                    }
                }
                if (str_len + 2 >= str_cap) {
                    str_cap *= 2;
                    str_buf = (char *)realloc(str_buf, str_cap);
                }
                str_buf[str_len++] = ch;
            }
            str_buf[str_len] = '\0';
            if (*p == '"') {
                p++; // skip closing quote
                col++;
            }

            Token tok;
            memset(&tok, 0, sizeof(tok));
            tok.type = TOK_STR;
            tok.text = str_buf;
            tok.line = line;
            tok.col = start_col;
            token_array_push(ta, tok);
            line_has_tokens = true;
            continue;
        }

        // Identifiers and numbers
        int start_col = col;
        const char *word_start = p;
        while (*p && is_ident_char(*p)) {
            p++;
            col++;
        }
        size_t word_len = p - word_start;
        char *word = (char *)malloc(word_len + 1);
        memcpy(word, word_start, word_len);
        word[word_len] = '\0';

        Token tok;
        memset(&tok, 0, sizeof(tok));
        tok.line = line;
        tok.col = start_col;
        tok.text = word;

        int64_t ival = 0;
        double fval = 0.0;
        bool is_float = false;

        if (parse_number(word, &ival, &fval, &is_float)) {
            if (is_float) {
                tok.type = TOK_FLOAT;
                tok.float_val = fval;
            } else {
                tok.type = TOK_INT;
                tok.int_val = ival;
            }
        } else {
            tok.type = TOK_IDENT;
        }

        token_array_push(ta, tok);
        line_has_tokens = true;
    }

    // If last line had non-empty tokens, emit a final TOK_NL
    if (line_has_tokens) {
        Token tok;
        memset(&tok, 0, sizeof(tok));
        tok.type = TOK_NL;
        tok.text = strdup("<NL>");
        tok.line = line;
        tok.col = col;
        token_array_push(ta, tok);
    }

    // Emit TOK_EOF
    Token eof_tok;
    memset(&eof_tok, 0, sizeof(eof_tok));
    eof_tok.type = TOK_EOF;
    eof_tok.text = strdup("<EOF>");
    eof_tok.line = line;
    eof_tok.col = col;
    token_array_push(ta, eof_tok);

    return ta;
}
