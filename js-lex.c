#include "js.h"

#define nelem(a) (sizeof (a) / sizeof (a)[0])

struct {
	const char *string;
	js_Token token;
} keywords[] = {
	{"abstract", JS_ABSTRACT},
	{"boolean", JS_BOOLEAN},
	{"break", JS_BREAK},
	{"byte", JS_BYTE},
	{"case", JS_CASE},
	{"catch", JS_CATCH},
	{"char", JS_CHAR},
	{"class", JS_CLASS},
	{"const", JS_CONST},
	{"continue", JS_CONTINUE},
	{"debugger", JS_DEBUGGER},
	{"default", JS_DEFAULT},
	{"delete", JS_DELETE},
	{"do", JS_DO},
	{"double", JS_DOUBLE},
	{"else", JS_ELSE},
	{"enum", JS_ENUM},
	{"export", JS_EXPORT},
	{"extends", JS_EXTENDS},
	{"false", JS_FALSE},
	{"final", JS_FINAL},
	{"finally", JS_FINALLY},
	{"float", JS_FLOAT},
	{"for", JS_FOR},
	{"function", JS_FUNCTION},
	{"goto", JS_GOTO},
	{"if", JS_IF},
	{"implements", JS_IMPLEMENTS},
	{"import", JS_IMPORT},
	{"in", JS_IN},
	{"instanceof", JS_INSTANCEOF},
	{"int", JS_INT},
	{"interface", JS_INTERFACE},
	{"long", JS_LONG},
	{"native", JS_NATIVE},
	{"new", JS_NEW},
	{"null", JS_NULL},
	{"package", JS_PACKAGE},
	{"private", JS_PRIVATE},
	{"protected", JS_PROTECTED},
	{"public", JS_PUBLIC},
	{"return", JS_RETURN},
	{"short", JS_SHORT},
	{"static", JS_STATIC},
	{"super", JS_SUPER},
	{"switch", JS_SWITCH},
	{"synchronized", JS_SYNCHRONIZED},
	{"this", JS_THIS},
	{"throw", JS_THROW},
	{"throws", JS_THROWS},
	{"transient", JS_TRANSIENT},
	{"true", JS_TRUE},
	{"try", JS_TRY},
	{"typeof", JS_TYPEOF},
	{"var", JS_VAR},
	{"void", JS_VOID},
	{"volatile", JS_VOLATILE},
	{"while", JS_WHILE},
	{"with", JS_WITH},
};

const char *tokenstrings[] = {
	"ERROR", "EOF", "(identifier)", "null", "true", "false", "(number)",
	"(string)", "(regexp)", "\\n", "{", "}", "(", ")", "[", "]", ".", ";",
	",", "<", ">", "<=", ">=", "==", "!=", "===", "!==", "+", "-", "*",
	"%", "++", "--", "<<", ">>", ">>>", "&", "|", "^", "!", "~", "&&",
	"||", "?", ":", "=", "+=", "-=", "*=", "%=", "<<=", ">>=", ">>>=",
	"&=", "|=", "^=", "/", "/=", "break", "case", "catch", "continue",
	"default", "delete", "do", "else", "finally", "for", "function", "if",
	"in", "instanceof", "new", "return", "switch", "this", "throw", "try",
	"typeof", "var", "void", "while", "with", "abstract", "boolean",
	"byte", "char", "class", "const", "debugger", "double", "enum",
	"export", "extends", "final", "float", "goto", "implements", "import",
	"int", "interface", "long", "native", "package", "private",
	"protected", "public", "short", "static", "super", "synchronized",
	"throws", "transient", "volatile",
};

const char *js_tokentostring(js_Token t)
{
	return tokenstrings[t];
}

static inline js_Token findkeyword(const char *s)
{
	int m, l, r;
	int c;

	l = 0;
	r = nelem(keywords) - 1;

	while (l <= r) {
		m = (l + r) >> 1;
		c = strcmp(s, keywords[m].string);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return keywords[m].token;
	}

	return JS_IDENTIFIER;
}

static inline int iswhite(int c)
{
	return c == 0x9 || c == 0xb || c == 0xc || c == 0x20 || c == 0xa0;
}

static inline int isnewline(c)
{
	return c == 0xa || c == 0xd || c == 0x2028 || c == 0x2029;
}

#define GETC() *(*sp)++
#define UNGETC() (*sp)--
#define LOOK(x) (**sp == x ? *(*sp)++ : 0)

static inline void lexlinecomment(const char **sp)
{
	int c = GETC();
	while (!isnewline(c))
		c = GETC();
	UNGETC();
}

static inline int lexcomment(const char **sp)
{
	while (1) {
		int c = GETC();
		if (c == '*') {
			while (c == '*')
				c = GETC();
			if (c == '/')
				return 0;
		} else if (c == 0) {
			return -1;
		}
	}
}

static inline int isidentifierstart(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '$' || c == '_';
}

static inline int isidentifierpart(int c)
{
	return (c >= '0' && c <= '9') || isidentifierstart(c);
}

static inline int isdec(int c)
{
	return (c >= '0' && c <= '9');
}

static inline int ishex(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline int tohex(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0xa;
	return 0;
}

static inline js_Token lexhex(const char **sp, double *yynumber)
{
	int c = GETC();
	double n = 0;

	if (!ishex(c))
		return JS_ERROR;

	do {
		n = n * 16 + tohex(c);
		c = GETC();
	} while (ishex(c));

	UNGETC();
	*yynumber = n;

	return JS_NUMBER;
}

static inline double lexinteger(const char **sp)
{
	int c = GETC();
	double n = 0;

	while (isdec(c)) {
		n = n * 10 + (c - '0');
		c = GETC();
	}

	UNGETC();

	return n;
}

static inline double lexfraction(const char **sp)
{
	int c = GETC();
	double n = 0;
	double d = 1;

	while (isdec(c)) {
		n = n * 10 + (c - '0');
		d = d * 10;
		c = GETC();
	}

	UNGETC();

	return n / d;
}

static inline js_Token lexnumber(int c, const char **sp, double *yynumber)
{
	double i, f, e;

	if (c == '0' && (LOOK('x') || LOOK('X')))
		return lexhex(sp, yynumber);

	UNGETC();

	i = lexinteger(sp);

	f = 0;
	if (LOOK('.'))
		f = lexfraction(sp);

	e = 0;
	if (LOOK('e') || LOOK('E')) {
		if (LOOK('-'))
			e = -lexinteger(sp);
		else if (LOOK('+'))
			e = lexinteger(sp);
		else
			e = lexinteger(sp);
	}

	*yynumber = (i + f) * pow(10, e);

	return JS_NUMBER;
}

static inline int lexescape(const char **sp)
{
	int c = GETC();
	int x, y, z, w;

	switch (c) {
	case '0': return 0;
	case 'u':
		x = tohex(GETC());
		y = tohex(GETC());
		z = tohex(GETC());
		w = tohex(GETC());
		return (x << 12) | (y << 8) | (z << 4) | w;
	case 'x':
		x = tohex(GETC());
		y = tohex(GETC());
		return (x << 4) | y;
	case '\'': return '\'';
	case '"': return '"';
	case '\\': return '\\';
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	default: return c;
	}
}

static inline js_Token lexstring(int q, const char **sp, char *yytext, size_t yylen)
{
	char *p = yytext;
	int c = GETC();

	while (c != q) {
		if (c == 0 || isnewline(c))
			return JS_ERROR;

		if (c == '\\')
			c = lexescape(sp);

		if (p - yytext >= yylen)
			return JS_ERROR;
		*p++ = c;
		c = GETC();
	}

	*p = 0;

	return JS_STRING;
}

js_Token js_lex(js_State *J, const char **sp, char *yytext, size_t yylen, double *yynumber)
{
	int c = GETC();

	while (c) {
		while (iswhite(c))
			c = GETC();

		if (isnewline(c))
			return JS_NEWLINE;

		if (c == '/') {
			c = GETC();
			if (c == '/') {
				lexlinecomment(sp);
			} else if (c == '*') {
				if (lexcomment(sp))
					return JS_ERROR;
			} else if (c == '=') {
				return JS_SLASH_EQ;
			} else {
				UNGETC();
				return JS_SLASH;
			}
		}

		if (isidentifierstart(c)) {
			char *p = yytext;

			do {
				if (p - yytext >= yylen)
					return JS_ERROR;
				*p++ = c;
				c = GETC();
			} while (isidentifierpart(c));

			UNGETC();
			*p = 0;

			return findkeyword(yytext);
		}

		if ((c >= '0' && c <= '9') || c == '.')
			return lexnumber(c, sp, yynumber);

		if (c == '\'' || c == '"')
			return lexstring(c, sp, yytext, yylen);

		switch (c) {
		case '{': return JS_LCURLY;
		case '}': return JS_RCURLY;
		case '(': return JS_LPAREN;
		case ')': return JS_RPAREN;
		case '[': return JS_LSQUARE;
		case ']': return JS_RSQUARE;
		case '.': return JS_PERIOD;
		case ';': return JS_SEMICOLON;
		case ',': return JS_COMMA;

		case '<':
			if (LOOK('<')) {
				if (LOOK('='))
					return JS_LT_LT_EQ;
				return JS_LT_LT;
			}
			if (LOOK('='))
				return JS_LT_EQ;
			return JS_LT;

		case '>':
			if (LOOK('>')) {
				if (LOOK('>')) {
					if (LOOK('='))
						return JS_GT_GT_GT_EQ;
					return JS_GT_GT_GT;
				}
				if (LOOK('='))
					return JS_GT_GT_EQ;
				return JS_GT_GT;
			}
			if (LOOK('='))
				return JS_GT_EQ;
			return JS_GT;

		case '=':
			if (LOOK('=')) {
				if (LOOK('='))
					return JS_EQ_EQ_EQ;
				return JS_EQ_EQ;
			}
			return JS_EQ;

		case '!':
			if (LOOK('=')) {
				if (LOOK('='))
					return JS_EXCL_EQ_EQ;
				return JS_EXCL_EQ;
			}
			return JS_EXCL;

		case '+':
			if (LOOK('+'))
				return JS_PLUS_PLUS;
			if (LOOK('='))
				return JS_PLUS_EQ;
			return JS_PLUS;

		case '-':
			if (LOOK('-'))
				return JS_MINUS_MINUS;
			if (LOOK('='))
				return JS_MINUS_EQ;
			return JS_MINUS;

		case '*':
			if (LOOK('='))
				return JS_STAR_EQ;
			return JS_STAR;

		case '%':
			if (LOOK('='))
				return JS_PERCENT_EQ;
			return JS_PERCENT;

		case '&':
			if (LOOK('&'))
				return JS_AND_AND;
			if (LOOK('='))
				return JS_AND_EQ;
			return JS_AND;

		case '|':
			if (LOOK('|'))
				return JS_BAR_BAR;
			if (LOOK('='))
				return JS_BAR_EQ;
			return JS_BAR;

		case '^':
			if (LOOK('='))
				return JS_HAT_EQ;
			return JS_HAT;

		case '~': return JS_TILDE;
		case '?': return JS_QUESTION;
		case ':': return JS_COLON;
		}

		c = GETC();
	}

	return JS_EOF;
}