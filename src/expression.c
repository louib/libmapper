#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapper_internal.h"

#define MAX_HISTORY -100
#define STACK_SIZE 128
#define N_USER_VARS 8
#ifdef DEBUG
#define TRACING 0 /* Set non-zero to see trace during parse & eval. */
#else
#define TRACING 0
#endif

#define lex_error trace
#define parse_error trace

typedef union _mapper_value {
    float f;
    double d;
    int i32;
} mapper_value_t;

static int mini(int x, int y)
{
    if (y < x) return y;
    else return x;
}

static float minf(float x, float y)
{
    if (y < x) return y;
    else return x;
}

static double mind(double x, double y)
{
    if (y < x) return y;
    else return x;
}

static int maxi(int x, int y)
{
    if (y > x) return y;
    else return x;
}

static float maxf(float x, float y)
{
    if (y > x) return y;
    else return x;
}

static double maxd(double x, double y)
{
    if (y > x) return y;
    else return x;
}

static float pif()
{
    return M_PI;
}

static double pid()
{
    return M_PI;
}

static float ef()
{
    return M_E;
}

static double ed()
{
    return M_E;
}

static float midiToHzf(float x)
{
    return 440. * pow(2.0, (x - 69) / 12.0);
}

static double midiToHzd(double x)
{
    return 440. * pow(2.0, (x - 69) / 12.0);
}

static float hzToMidif(float x)
{
    return 69. + 12. * log2(x / 440.);
}

static double hzToMidid(double x)
{
    return 69. + 12. * log2(x / 440.);
}

static float uniformf(float x)
{
    return rand() / (RAND_MAX + 1.0) * x;
}

static double uniformd(double x)
{
    return rand() / (RAND_MAX + 1.0) * x;
}

static int alli(mapper_value_t *val, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (val[i].i32 == 0) {
            return 0;
        }
    }
    return 1;
}

static int allf(mapper_value_t *val, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (val[i].f == 0) {
            return 0;
        }
    }
    return 1;
}

static int alld(mapper_value_t *val, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (val[i].d == 0) {
            return 0;
        }
    }
    return 1;
}


static int anyi(mapper_value_t *val, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (val[i].i32 != 0) {
            return 1;
        }
    }
    return 0;
}

static float anyf(mapper_value_t *val, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (val[i].f != 0.f) {
            return 1;
        }
    }
    return 0;
}

static double anyd(mapper_value_t *val, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (val[i].d != 0.) {
            return 1;
        }
    }
    return 0;
}

static int sumi(mapper_value_t *val, int length)
{
    int i, aggregate = 0;
    for (i = 0; i < length; i++) {
        aggregate += val[i].i32;
    }
    return aggregate;
}

static float sumf(mapper_value_t *val, int length)
{
    int i;
    float aggregate = 0.f;
    for (i = 0; i < length; i++) {
        aggregate += val[i].f;
    }
    return aggregate;
}

static double sumd(mapper_value_t *val, int length)
{
    int i;
    double aggregate = 0.;
    for (i = 0; i < length; i++) {
        aggregate += val[i].d;
    }
    return aggregate;
}

static float meanf(mapper_value_t *val, int length)
{
    return sumf(val, length) / (float)length;
}

static double meand(mapper_value_t *val, int length)
{
    return sumd(val, length) / (double)length;
}

static int vmaxi(mapper_value_t *val, int length)
{
    int i, max = val[0].i32;
    for (i = 1; i < length; i++) {
        if (val[i].i32 > max)
            max = val[i].i32;
    }
    return max;
}

static float vmaxf(mapper_value_t *val, int length)
{
    int i;
    float max = val[0].f;
    for (i = 1; i < length; i++) {
        if (val[i].f > max)
            max = val[i].f;
    }
    return max;
}

static double vmaxd(mapper_value_t *val, int length)
{
    int i;
    double max = val[0].d;
    for (i = 1; i < length; i++) {
        if (val[i].d > max)
            max = val[i].d;
    }
    return max;
}

static int vmini(mapper_value_t *val, int length)
{
    int i, min = val[0].i32;
    for (i = 1; i < length; i++) {
        if (val[i].i32 < min)
            min = val[i].i32;
    }
    return min;
}

static float vminf(mapper_value_t *val, int length)
{
    int i;
    float min = val[0].f;
    for (i = 1; i < length; i++) {
        if (val[i].f < min)
            min = val[i].f;
    }
    return min;
}

static double vmind(mapper_value_t *val, int length)
{
    int i;
    double min = val[0].d;
    for (i = 1; i < length; i++) {
        if (val[i].d < min)
            min = val[i].d;
    }
    return min;
}

static float emaf(float memory, float val, float weight)
{
    return val * weight + memory * (1 - weight);
}

static double emad(double memory, double val, double weight)
{
    return val * weight + memory * (1 - weight);
}

static float schmittf(float memory, float val, float low, float high)
{
    return memory ? val > low : val >= high;
}

static double schmittd(double memory, double val, double low, double high)
{
    return memory ? val > low : val >= high;
}

typedef enum {
    VAR_UNKNOWN=-1,
    VAR_Y=N_USER_VARS,
    VAR_X,
    N_VARS
} expr_var_t;

typedef enum {
    FUNC_UNKNOWN=-1,
    FUNC_ABS=0,
    FUNC_ACOS,
    FUNC_ACOSH,
    FUNC_ASIN,
    FUNC_ASINH,
    FUNC_ATAN,
    FUNC_ATAN2,
    FUNC_ATANH,
    FUNC_CBRT,
    FUNC_CEIL,
    FUNC_COS,
    FUNC_COSH,
    FUNC_E,
    FUNC_EMA,
    FUNC_EXP,
    FUNC_EXP2,
    FUNC_FLOOR,
    FUNC_HYPOT,
    FUNC_HZTOMIDI,
    FUNC_LOG,
    FUNC_LOG10,
    FUNC_LOG2,
    FUNC_LOGB,
    FUNC_MAX,
    FUNC_MIDITOHZ,
    FUNC_MIN,
    FUNC_PI,
    FUNC_POW,
    FUNC_ROUND,
    FUNC_SCHMITT,
    FUNC_SIN,
    FUNC_SINH,
    FUNC_SQRT,
    FUNC_TAN,
    FUNC_TANH,
    FUNC_TRUNC,
    /* place functions which should never be precomputed below this point */
    FUNC_UNIFORM,
    N_FUNCS
} expr_func_t;

static struct {
    const char *name;
    const char arity;
    const char memory;
    void *func_int32;
    void *func_float;
    void *func_double;
} function_table[] = {
    { "abs",        1,  0,  abs,    fabsf,      fabs        },
    { "acos",       1,  0,  0,      acosf,      acos        },
    { "acosh",      1,  0,  0,      acoshf,     acosh       },
    { "asin",       1,  0,  0,      asinf,      asin        },
    { "asinh",      1,  0,  0,      asinhf,     asinh       },
    { "atan",       1,  0,  0,      atanf,      atan        },
    { "atan2",      2,  0,  0,      atan2f,     atan2       },
    { "atanh",      1,  0,  0,      atanhf,     atanh       },
    { "cbrt",       1,  0,  0,      cbrtf,      cbrt        },
    { "ceil",       1,  0,  0,      ceilf,      ceil        },
    { "cos",        1,  0,  0,      cosf,       cos         },
    { "cosh",       1,  0,  0,      coshf,      cosh        },
    { "e",          0,  0,  0,      ef,         ed          },
    { "ema",        3,  1,  0,      emaf,       emad        },
    { "exp",        1,  0,  0,      expf,       exp         },
    { "exp2",       1,  0,  0,      exp2f,      exp2        },
    { "floor",      1,  0,  0,      floorf,     floor       },
    { "hypot",      2,  0,  0,      hypotf,     hypot       },
    { "hzToMidi",   1,  0,  0,      hzToMidif,  hzToMidid   },
    { "log",        1,  0,  0,      logf,       log         },
    { "log10",      1,  0,  0,      log10f,     log10       },
    { "log2",       1,  0,  0,      log2f,      log2        },
    { "logb",       1,  0,  0,      logbf,      logb        },
    { "max",        2,  0,  maxi,   maxf,       maxd        },
    { "midiToHz",   1,  0,  0,      midiToHzf,  midiToHzd   },
    { "min",        2,  0,  mini,   minf,       mind        },
    { "pi",         0,  0,  0,      pif,        pid         },
    { "pow",        2,  0,  0,      powf,       pow         },
    { "round",      1,  0,  0,      roundf,     round       },
    { "schmitt",    4,  1,  0,      schmittf,   schmittd    },
    { "sin",        1,  0,  0,      sinf,       sin         },
    { "sinh",       1,  0,  0,      sinhf,      sinh        },
    { "sqrt",       1,  0,  0,      sqrtf,      sqrt        },
    { "tan",        1,  0,  0,      tanf,       tan         },
    { "tanh",       1,  0,  0,      tanhf,      tanh        },
    { "trunc",      1,  0,  0,      truncf,     trunc       },
    /* place functions which should never be precomputed below this point */
    { "uniform",    1,  0,  0,      uniformf,   uniformd    },
};

typedef enum {
    VFUNC_UNKNOWN=-1,
    VFUNC_ALL=0,
    VFUNC_ANY,
    VFUNC_MEAN,
    VFUNC_SUM,
    VFUNC_MAX,
    VFUNC_MIN,
    N_VFUNCS
} expr_vfunc_t;

static struct {
    const char *name;
    unsigned int arity;
    void *func_int32;
    void *func_float;
    void *func_double;
} vfunction_table[] = {
    { "all",    1,      alli,       allf,       alld        },
    { "any",    1,      anyi,       anyf,       anyd        },
    { "mean",   1,      0,          meanf,      meand       },
    { "sum",    1,      sumi,       sumf,       sumd        },
    { "max",    1,      vmaxi,      vmaxf,      vmaxd       },
    { "min",    1,      vmini,      vminf,      vmind       },
};

typedef enum {
    OP_LOGICAL_NOT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_ADD,
    OP_SUBTRACT,
    OP_LEFT_BIT_SHIFT,
    OP_RIGHT_BIT_SHIFT,
    OP_IS_GREATER_THAN,
    OP_IS_GREATER_THAN_OR_EQUAL,
    OP_IS_LESS_THAN,
    OP_IS_LESS_THAN_OR_EQUAL,
    OP_IS_EQUAL,
    OP_IS_NOT_EQUAL,
    OP_BITWISE_AND,
    OP_BITWISE_XOR,
    OP_BITWISE_OR,
    OP_LOGICAL_AND,
    OP_LOGICAL_OR,
    OP_CONDITIONAL_IF_THEN,
    OP_CONDITIONAL_IF_ELSE,
    OP_CONDITIONAL_IF_THEN_ELSE,
} expr_op_t;

#define NONE        0x0
#define GET_ZERO    0x1
#define GET_ONE     0x2
#define GET_OPER    0x4
#define BAD_EXPR    0x8

static struct {
    const char *name;
    char arity;
    char precedence;
    uint16_t optimize_const_operands;
} op_table[] = {
/*                     left==0  | right==0     | left==1      | right==1     */
    { "!",      1, 11, GET_ONE  | GET_ONE  <<4 | GET_ZERO <<8 | GET_ZERO <<12 },
    { "*",      2, 10, GET_ZERO | GET_ZERO <<4 | GET_OPER <<8 | GET_OPER <<12 },
    { "/",      2, 10, GET_ZERO | BAD_EXPR <<4 | NONE     <<8 | GET_OPER <<12 },
    { "%",      2, 10, GET_ZERO | GET_OPER <<4 | GET_ONE  <<8 | GET_OPER <<12 },
    { "+",      2, 9,  GET_OPER | GET_OPER <<4 | NONE     <<8 | NONE     <<12 },
    { "-",      2, 9,  NONE     | GET_OPER <<4 | NONE     <<8 | NONE     <<12 },
    { "<<",     2, 8,  GET_ZERO | GET_OPER <<4 | NONE     <<8 | NONE     <<12 },
    { ">>",     2, 8,  GET_ZERO | GET_OPER <<4 | NONE     <<8 | NONE     <<12 },
    { ">",      2, 7,  NONE     | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { ">=",     2, 7,  NONE     | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "<",      2, 7,  NONE     | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "<=",     2, 7,  NONE     | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "==",     2, 6,  NONE     | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "!=",     2, 6,  NONE     | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "&",      2, 5,  GET_ZERO | GET_ZERO <<4 | NONE     <<8 | NONE     <<12 },
    { "^",      2, 4,  GET_OPER | GET_OPER <<4 | NONE     <<8 | NONE     <<12 },
    { "|",      2, 3,  GET_OPER | GET_OPER <<4 | GET_ONE  <<8 | GET_ONE  <<12 },
    { "&&",     2, 2,  GET_ZERO | GET_ZERO <<4 | NONE     <<8 | NONE     <<12 },
    { "||",     2, 1,  GET_OPER | GET_OPER <<4 | GET_ONE  <<8 | GET_ONE  <<12 },
    // TODO: handle optimization of ternary operator
    { "IFTHEN",      2, 0, NONE | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "IFELSE",      2, 0, NONE | NONE     <<4 | NONE     <<8 | NONE     <<12 },
    { "IFTHENELSE",  3, 0, NONE | NONE     <<4 | NONE     <<8 | NONE     <<12 },
};

typedef int func_int32_arity0();
typedef int func_int32_arity1(int);
typedef int func_int32_arity2(int,int);
typedef float func_float_arity0();
typedef float func_float_arity1(float);
typedef float func_float_arity2(float,float);
typedef float func_float_arity3(float,float,float);
typedef float func_float_arity4(float,float,float,float);
typedef double func_double_arity0();
typedef double func_double_arity1(double);
typedef double func_double_arity2(double,double);
typedef double func_double_arity3(double,double,double);
typedef double func_double_arity4(double,double,double,double);
typedef int vfunc_int32_arity1(mapper_value_t*, int);
typedef float vfunc_float_arity1(mapper_value_t*, int);
typedef double vfunc_double_arity1(mapper_value_t*, int);

typedef struct _token {
    enum {
        TOK_CONST           = 0x0001,
        TOK_NEGATE          = 0x0002,
        TOK_FUNC            = 0x0003,
        TOK_VFUNC           = 0x0004,
        TOK_OPEN_PAREN      = 0x0005,
        TOK_OPEN_SQUARE     = 0x0010,
        TOK_OPEN_CURLY      = 0x0020,
        TOK_CLOSE_PAREN     = 0x0040,
        TOK_CLOSE_SQUARE    = 0x0080,
        TOK_CLOSE_CURLY     = 0x0100,
        TOK_VAR             = 0x0200,
        TOK_OP              = 0x0400,
        TOK_COMMA           = 0x0800,
        TOK_COLON           = 0x1000,
        TOK_SEMICOLON       = 0x2000,
        TOK_VECTORIZE       = 0x4000,
        TOK_ASSIGNMENT      = 0x8000,
        TOK_ASSIGN_USE,
        TOK_END,
    } toktype;
    union {
        float f;
        int i;
        double d;
        expr_var_t var;
        expr_op_t op;
        expr_func_t func;
        expr_vfunc_t vfunc;
    };
    union {
        char casttype;
        int assignment_offset;
    };
    int vector_length;
    union {
        int vector_index;
        int arity;
    };
    char history_index;
    char vector_length_locked;
    char datatype;
} mapper_token_t, *mapper_token;

typedef struct _variable {
    char *name;
    int vector_length;
    char datatype;
    char casttype;
    char history_size;
    char vector_length_locked;
    char assigned;
} mapper_variable_t, *mapper_variable;

static expr_func_t function_lookup(const char *s, int len)
{
    int i, j;
    for (i=0; i<N_FUNCS; i++) {
        if (strlen(function_table[i].name) == len
            && strncmp(s, function_table[i].name, len)==0) {
            if (!function_table[i].arity)
                return i;
            // also check for parenthesis
            j = strlen(function_table[i].name);
            while (s[j]) {
                if (s[j] == '(')
                    return i;
                else if (s[j] != ' ')
                    return FUNC_UNKNOWN;
                j++;
            }
            break;
        }
    }
    return FUNC_UNKNOWN;
}

static expr_vfunc_t vfunction_lookup(const char *s, int len)
{
    int i, j;
    for (i=0; i<N_VFUNCS; i++) {
        if (strlen(vfunction_table[i].name) == len
            && strncmp(s, vfunction_table[i].name, len)==0) {
            if (vfunction_table[i].arity)
                return i;
            // also check for parenthesis
            j = strlen(vfunction_table[i].name);
            while (s[j]) {
                if (s[j] == '(')
                    return i;
                else if (s[j] != ' ')
                    return VFUNC_UNKNOWN;
                j++;
            }
            break;
        }
    }
    return VFUNC_UNKNOWN;
}

static expr_var_t variable_lookup(const char *s, int len)
{
    // TODO: allow more than 10 source slots
    if (*s == 'y' && len == 1)
        return VAR_Y;
    if (*s == 'x') {
        if (len == 1)
            return VAR_X;
        if (len == 2 && isdigit(*(s+1)))
            return VAR_X + atoi(s+1);
    }
    return VAR_UNKNOWN;
}

static int const_tok_is_zero(mapper_token_t tok)
{
    switch (tok.datatype) {
        case 'i':
            return tok.i == 0;
        case 'f':
            return tok.f == 0.f;
        case 'd':
            return tok.d == 0.0;
    }
    return 0;
}

static int const_tok_is_one(mapper_token_t tok)
{
    switch (tok.datatype) {
        case 'i':
            return tok.i == 1;
        case 'f':
            return tok.f == 1.f;
        case 'd':
            return tok.d == 1.0;
    }
    return 0;
}

static int tok_arity(mapper_token_t tok)
{
    switch (tok.toktype) {
        case TOK_OP:
            return op_table[tok.op].arity;
        case TOK_FUNC:
            return function_table[tok.func].arity;
        case TOK_VFUNC:
            return vfunction_table[tok.func].arity;
        case TOK_VECTORIZE:
            return tok.arity;
        default:
            return 0;
    }
    return 0;
}

static int expr_lex(const char *str, int index, mapper_token_t *tok)
{
    tok->datatype = 'i';
    tok->casttype = 0;
    tok->vector_length = 1;
    tok->vector_index = 0;
    tok->vector_length_locked = 0;
    int n=index, i=index;
    char c = str[index];
    int integer_found = 0;

    if (c==0) {
        tok->toktype = TOK_END;
        return index;
    }

  again:

    i = index;
    if (isdigit(c)) {
        do {
            c = str[++index];
        } while (c && isdigit(c));
        n = atoi(str+i);
        integer_found = 1;
        if (c!='.' && c!='e') {
            tok->i = n;
            tok->toktype = TOK_CONST;
            tok->datatype = 'i';
            return index;
        }
    }

    switch (c) {
    case '.':
        c = str[++index];
        if (!isdigit(c) && c!='e' && integer_found) {
            tok->toktype = TOK_CONST;
            tok->f = (float)n;
            tok->datatype = 'f';
            return index;
        }
        if (!isdigit(c) && c!='e')
            break;
        do {
            c = str[++index];
        } while (c && isdigit(c));
        if (c!='e') {
            tok->f = atof(str+i);
            tok->toktype = TOK_CONST;
            tok->datatype = 'f';
            return index;
        }
    case 'e':
        if (!integer_found) {
            n = index;
            while (c && (isalpha(c) || isdigit(c)))
                c = str[++index];
            tok->toktype = TOK_FUNC;
            if ((tok->func = function_lookup(str+i, index-i)) != FUNC_UNKNOWN) {
                tok->toktype = TOK_FUNC;
            }
            else if ((tok->vfunc = vfunction_lookup(str+i, index-i)) != VFUNC_UNKNOWN) {
                tok->toktype = TOK_VFUNC;
            }
            else {
                tok->var = variable_lookup(str+i, index-i);
                tok->toktype = TOK_VAR;
            }
            return index;
        }
        c = str[++index];
        if (c!='-' && c!='+' && !isdigit(c)) {
            lex_error("Incomplete scientific notation `%s'.\n", str+i);
            break;
        }
        if (c=='-' || c=='+')
            c = str[++index];
        while (c && isdigit(c))
            c = str[++index];
        tok->toktype = TOK_CONST;
        tok->datatype = 'f';
        tok->f = atof(str+i);
        return index;
    case '+':
        tok->toktype = TOK_OP;
        tok->op = OP_ADD;
        return ++index;
    case '-':
        // could be either subtraction or negation
        i = index-1;
        // back up one character
        while (i && strchr(" \t\r\n", str[i]))
           i--;
        if (isalpha(str[i]) || isdigit(str[i]) || strchr(")]}", str[i])) {
            tok->toktype = TOK_OP;
            tok->op = OP_SUBTRACT;
        }
        else {
            tok->toktype = TOK_NEGATE;
        }
        return ++index;
    case '/':
        tok->toktype = TOK_OP;
        tok->op = OP_DIVIDE;
        return ++index;
    case '*':
        tok->toktype = TOK_OP;
        tok->op = OP_MULTIPLY;
        return ++index;
    case '%':
        tok->toktype = TOK_OP;
        tok->op = OP_MODULO;
        return ++index;
    case '=':
        // could be '=', '=='
        c = str[++index];
        if (c == '=') {
            tok->toktype = TOK_OP;
            tok->op = OP_IS_EQUAL;
            ++index;
        }
        else {
            tok->toktype = TOK_ASSIGNMENT;
        }
        return index;
    case '<':
        // could be '<', '<=', '<<'
        tok->toktype = TOK_OP;
        tok->op = OP_IS_LESS_THAN;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_LESS_THAN_OR_EQUAL;
            ++index;
        }
        else if (c == '<') {
            tok->op = OP_LEFT_BIT_SHIFT;
            ++index;
        }
        return index;
    case '>':
        // could be '>', '>=', '>>'
        tok->toktype = TOK_OP;
        tok->op = OP_IS_GREATER_THAN;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_GREATER_THAN_OR_EQUAL;
            ++index;
        }
        else if (c == '>') {
            tok->op = OP_RIGHT_BIT_SHIFT;
            ++index;
        }
        return index;
    case '!':
        // could be '!', '!='
        // TODO: handle factorial case
        tok->toktype = TOK_OP;
        tok->op = OP_LOGICAL_NOT;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_NOT_EQUAL;
            ++index;
        }
        return index;
    case '&':
        // could be '&', '&&'
        tok->toktype = TOK_OP;
        tok->op = OP_BITWISE_AND;
        c = str[++index];
        if (c == '&') {
            tok->op = OP_LOGICAL_AND;
            ++index;
        }
        return index;
    case '|':
        // could be '|', '||'
        tok->toktype = TOK_OP;
        tok->op = OP_BITWISE_OR;
        c = str[++index];
        if (c == '|') {
            tok->op = OP_LOGICAL_OR;
            ++index;
        }
        return index;
    case '^':
        // bitwise XOR
        tok->toktype = TOK_OP;
        tok->op = OP_BITWISE_XOR;
        return ++index;
    case '(':
        tok->toktype = TOK_OPEN_PAREN;
        return ++index;
    case ')':
        tok->toktype = TOK_CLOSE_PAREN;
        return ++index;
    case '[':
        tok->toktype = TOK_OPEN_SQUARE;
        return ++index;
    case ']':
        tok->toktype = TOK_CLOSE_SQUARE;
        return ++index;
    case '{':
        tok->toktype = TOK_OPEN_CURLY;
        return ++index;
    case '}':
        tok->toktype = TOK_CLOSE_CURLY;
        return ++index;
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        c = str[++index];
        goto again;
    case ',':
        tok->toktype = TOK_COMMA;
        return ++index;
    case '?':
        // conditional
        tok->toktype = TOK_OP;
        tok->op = OP_CONDITIONAL_IF_THEN;
        c = str[++index];
        if (c == ':') {
            tok->op = OP_CONDITIONAL_IF_ELSE;
            ++index;
        }
        return index;
    case ':':
        tok->toktype = TOK_COLON;
        return ++index;
    case ';':
        tok->toktype = TOK_SEMICOLON;
        return ++index;
    default:
        if (!isalpha(c)) {
            lex_error("unknown character '%c' in lexer\n", c);
            break;
        }
        while (c && (isalpha(c) || isdigit(c)))
            c = str[++index];
        if ((tok->func = function_lookup(str+i, index-i)) != FUNC_UNKNOWN) {
            tok->toktype = TOK_FUNC;
        }
        else if ((tok->vfunc = vfunction_lookup(str+i, index-i)) != VFUNC_UNKNOWN) {
            tok->toktype = TOK_VFUNC;
        }
        else {
            tok->var = variable_lookup(str+i, index-i);
            tok->toktype = TOK_VAR;
        }
        return index;
    }

    return 1;
}

struct _mapper_expr
{
    mapper_token tokens;
    mapper_token start;
    mapper_variable variables;
    int start_offset;
    int length;
    int vector_size;
    int input_history_size;
    int output_history_size;
    int num_variables;
    int constant_output;
};

void mapper_expr_free(mapper_expr expr)
{
    int i;
    if (expr->tokens)
        free(expr->tokens);
    if (expr->num_variables && expr->variables) {
        for (i = 0; i < expr->num_variables; i++) {
            free(expr->variables[i].name);
        }
        free(expr->variables);
    }
    free(expr);
}

#ifdef DEBUG

void printtoken(mapper_token_t tok)
{
    int i;
    char tokstr[32];
    switch (tok.toktype) {
        case TOK_CONST:
            switch (tok.datatype) {
                case 'f':   snprintf(tokstr, 32, "%f", tok.f);  break;
                case 'd':   snprintf(tokstr, 32, "%f", tok.d);  break;
                case 'i':   snprintf(tokstr, 32, "%d", tok.i);  break;
            }                                                   break;
        case TOK_OP:
            snprintf(tokstr, 32, "%s", op_table[tok.op].name);  break;
        case TOK_OPEN_CURLY:    snprintf(tokstr, 32, "{");      break;
        case TOK_OPEN_PAREN:    snprintf(tokstr, 32, "(");      break;
        case TOK_OPEN_SQUARE:   snprintf(tokstr, 32, "[");      break;
        case TOK_CLOSE_CURLY:   snprintf(tokstr, 32, "}");      break;
        case TOK_CLOSE_PAREN:   snprintf(tokstr, 32, ")");      break;
        case TOK_CLOSE_SQUARE:  snprintf(tokstr, 32, "]");      break;
        case TOK_VAR:
            if (tok.var == VAR_Y)
                snprintf(tokstr, 32, "y{%d}[%d]", tok.history_index,
                         tok.vector_index);
            else if (tok.var == VAR_X)
                snprintf(tokstr, 32, "x{%d}[%d]", tok.history_index,
                         tok.vector_index);
            else if (tok.var > VAR_X)
                snprintf(tokstr, 32, "x%d{%d}[%d]", tok.var - VAR_X,
                         tok.history_index, tok.vector_index);
            else
                snprintf(tokstr, 32, "var%d{%d}[%d]", tok.var,
                         tok.history_index, tok.vector_index);
            break;
        case TOK_FUNC:
            snprintf(tokstr, 32, "%s()", function_table[tok.func].name);
            break;
        case TOK_COMMA:         snprintf(tokstr, 32, ",");      break;
        case TOK_COLON:         snprintf(tokstr, 32, ":");      break;
        case TOK_VECTORIZE:
            snprintf(tokstr, 32, "VECT(%d)", tok.arity);
            break;
        case TOK_NEGATE:        snprintf(tokstr, 32, "-");      break;
        case TOK_VFUNC:
            snprintf(tokstr, 32, "%s()", vfunction_table[tok.func].name);
            break;
        case TOK_ASSIGNMENT:
        case TOK_ASSIGN_USE:
            if (tok.var == VAR_Y)
                snprintf(tokstr, 32, "ASSIGN_TO:y{%d}[%d]->[%d]",
                         tok.history_index, tok.assignment_offset,
                         tok.vector_index);
            else
                snprintf(tokstr, 32, "ASSIGN_TO:var%d{%d}[%d]->[%d]",
                         tok.var, tok.history_index,
                         tok.assignment_offset, tok.vector_index);
            break;
        case TOK_END:       printf("END\n");                    return;
        default:            printf("(unknown token)\n");        return;
    }
    printf("%s", tokstr);
    // indent
    int len = strlen(tokstr);
    for (i = len; i < 30; i++) {
        printf(" ");
    }
    printf("%c[%d]", tok.datatype, tok.vector_length);
    if (tok.vector_length_locked)
        printf("L");
    if (tok.toktype < TOK_ASSIGNMENT && tok.casttype)
        printf("->%c", tok.casttype);
    printf("\n");
}

void printstack(const char *s, mapper_token_t *stack, int top)
{
    int i, j, indent = 0;
    printf("%s ", s);
    if (s)
        indent = strlen(s) + 1;
    for (i=0; i<=top; i++) {
        if (i != 0) {
            for (j=0; j<indent; j++)
                printf(" ");
        }
        printtoken(stack[i]);
    }
    if (!i)
        printf("\n");
}

void printexpr(const char *s, mapper_expr e)
{
    printstack(s, e->tokens, e->length-1);
}

#endif

static char compare_token_datatype(mapper_token_t tok, char type)
{
    // return the higher datatype
    if (tok.datatype == 'd' || type == 'd')
        return 'd';
    else if (tok.datatype == 'f' || type == 'f')
        return 'f';
    else
        return 'i';
}

static char promote_token_datatype(mapper_token_t *tok, char type)
{
    tok->casttype = 0;

    if (tok->datatype == type)
        return type;

    if (tok->toktype >= TOK_ASSIGNMENT) {
        if (tok->var >= VAR_Y) {
            // typecasting is not possible
            return tok->datatype;
        }
        else {
            // user-defined variable, can typecast
            tok->casttype = type;
            return type;
        }
    }

    if (tok->toktype == TOK_CONST) {
        // constants can be cast immediately
        if (tok->datatype == 'i') {
            if (type == 'f') {
                tok->f = (float)tok->i;
                tok->datatype = type;
            }
            else if (type == 'd') {
                tok->d = (double)tok->i;
                tok->datatype = type;
            }
        }
        else if (tok->datatype == 'f') {
            if (type == 'd') {
                tok->d = (double)tok->f;
                tok->datatype = type;
            }
            else if (type == 'i') {
                tok->casttype = type;
            }
        }
        else {
            tok->casttype = type;
        }
        return type;
    }
    else if (tok->toktype == TOK_VAR) {
        // we need to cast at runtime
        tok->casttype = type;
        return type;
    }
    else {
        if (tok->datatype == 'i' || type == 'd') {
            tok->datatype = type;
            return type;
        }
        else {
            tok->casttype = type;
            return tok->datatype;
        }
    }
    return type;
}

static void lock_vector_lengths(mapper_token_t *stack, int top)
{
    int i=top, arity=1;

    while ((i >= 0) && arity--) {
        stack[i].vector_length_locked = 1;
        if (stack[i].toktype == TOK_OP)
            arity += op_table[stack[i].op].arity;
        else if (stack[i].toktype == TOK_FUNC)
            arity += function_table[stack[i].func].arity;
        else if (stack[i].toktype == TOK_VECTORIZE)
            arity += stack[i].arity;
        i--;
    }
}

static int precompute(mapper_token_t *stack, int length, int vector_length)
{
    struct _mapper_expr e;
    e.start = stack;
    e.start_offset = 0;
    e.length = length;
    e.vector_size = vector_length;
    e.variables = 0;
    e.num_variables = 0;
    mapper_history_t h;

    void *v = malloc(mapper_type_size(stack[length-1].datatype) * vector_length);
    h.type = stack[length-1].datatype;
    h.value = v;
    h.position = -1;
    h.length = vector_length;
    h.size = 1;

    if (!mapper_expr_evaluate(&e, 0, 0, &h, 0, 0)) {
        free(v);
        return 0;
    }

    int i;
    switch (h.type) {
        case 'f':
            for (i = 0; i < vector_length; i++)
                stack[i].f = ((float*)v)[i];
            break;
        case 'd':
            for (i = 0; i < vector_length; i++)
                stack[i].d = ((double*)v)[i];
            break;
        case 'i':
            for (i = 0; i < vector_length; i++)
                stack[i].i = ((int*)v)[i];
            break;
        default:
            free(v);
            return 0;
        break;
    }
    for (i = 0; i < vector_length; i++) {
        stack[i].toktype = TOK_CONST;
        stack[i].datatype = stack[length-1].datatype;
    }
    free(v);
    return length-1;
}

static int check_types_and_lengths(mapper_token_t *stack, int top,
                                   mapper_variable_t *vars)
{
    // TODO: allow precomputation of const-only vectors
    int i, arity, can_precompute = 1, optimize = NONE;
    char type = stack[top].datatype;
    int vector_length = stack[top].vector_length;

    switch (stack[top].toktype) {
        case TOK_OP:
            arity = op_table[stack[top].op].arity;
            break;
        case TOK_FUNC:
            arity = function_table[stack[top].func].arity;
            if (stack[top].func >= FUNC_UNIFORM)
                can_precompute = 0;
            break;
        case TOK_VFUNC:
            arity = vfunction_table[stack[top].func].arity;
            break;
        case TOK_VECTORIZE:
            arity = stack[top].arity;
            can_precompute = 0;
            break;
        case TOK_ASSIGNMENT:
        case TOK_ASSIGN_USE:
            arity = 1;
            can_precompute = 0;
            break;
        default:
            return top;
    }

    if (arity) {
        // find operator or function inputs
        i = top;
        int skip = 0;
        int depth = arity;
        int operand = 0;
        // last arg of op or func is at top-1
        type = compare_token_datatype(stack[top-1], type);
        if (stack[top-1].vector_length > vector_length)
            vector_length = stack[top-1].vector_length;

        /* Walk down stack distance of arity, checking datatypes
         * and vector lengths. */
        while (--i >= 0) {
            if (stack[i].toktype == TOK_FUNC &&
                function_table[stack[i].func].arity)
                can_precompute = 0;
            else if (stack[i].toktype != TOK_CONST)
                can_precompute = 0;

            if (skip == 0) {
                if (stack[i].toktype == TOK_CONST
                    && stack[top].toktype == TOK_OP
                    && depth <= op_table[stack[top].op].arity) {
                    if (const_tok_is_zero(stack[i])) {
                        // mask and bitshift, depth == 1 or 2
                        optimize = (op_table[stack[top].op].optimize_const_operands
                                    >>(depth-1)*4) & 0xF;
                    }
                    else if (const_tok_is_one(stack[i])) {
                        optimize = (op_table[stack[top].op].optimize_const_operands
                                    >>(depth+1)*4) & 0xF;
                    }
                    if (optimize == GET_OPER) {
                        if (i == top-1) {
                            // optimize immediately without moving other operand
                            return top-2;
                        }
                        else {
                            // store position of non-zero operand
                            operand = top-1;
                        }
                    }
                }
                type = compare_token_datatype(stack[i], type);
                if (stack[i].toktype == TOK_VFUNC)
                    stack[i].vector_length = vector_length;
                else if (stack[i].vector_length > vector_length)
                    vector_length = stack[i].vector_length;
                depth--;
                if (depth == 0)
                    break;
            }
            else
                skip--;

            switch (stack[i].toktype) {
                case TOK_OP:
                    skip += op_table[stack[i].op].arity;
                    break;
                case TOK_FUNC:
                    skip += function_table[stack[i].func].arity;
                    break;
                case TOK_VFUNC:
                    skip += vfunction_table[stack[i].func].arity;
                    break;
                case TOK_VECTORIZE:
                    skip += stack[i].arity;
                    break;
                case TOK_ASSIGN_USE:
                    skip++;
                    break;
                default:
                    break;
            }
        }

        if (depth)
            return -1;

        if (!can_precompute) {
            switch (optimize) {
                case BAD_EXPR:
                {
                    parse_error("Operator '%s' cannot have zero operand.\n",
                                op_table[stack[top].op].name);
                    return -1;
                }
                case GET_ZERO:
                case GET_ONE:
                {
                    // finish walking down compound arity
                    int _arity = 0;
                    while ((_arity += tok_arity(stack[i])) && i >= 0) {
                        _arity--;
                        i--;
                    }
                    stack[i].toktype = TOK_CONST;
                    stack[i].datatype = 'i';
                    stack[i].i = optimize == GET_ZERO ? 0 : 1;
                    stack[i].vector_length = vector_length;
                    stack[i].vector_length_locked = 0;
                    stack[i].casttype = 0;
                    return i;
                }
                case GET_OPER:
                    // copy tokens for non-zero operand
                    for (; i < operand; i++) {
                        // shunt tokens down stack
                        memcpy(stack+i, stack+i+1, sizeof(mapper_token_t));
                    }
                    // we may need to promote vector length, so do not return yet
                    top = operand-1;
                default:
                    break;
            }
        }

        /* walk down stack distance of arity again, promoting datatypes
         * and vector lengths */
        i = top;
        switch (stack[top].toktype) {
            case TOK_VECTORIZE:
                skip = stack[top].arity;
                depth = 0;
                break;
            case TOK_VFUNC:
            case TOK_ASSIGN_USE:
                skip = 1;
                depth = 0;
                break;
            default:
                skip = 0;
                depth = arity;
                break;
        }
        type = promote_token_datatype(&stack[i], type);
        while (--i >= 0) {
            // we will promote types within range of compound arity
            type = promote_token_datatype(&stack[i], type);

            if (skip <= 0) {
                // also check/promote vector length
                if (!stack[i].vector_length_locked) {
                    if (stack[i].toktype == TOK_VFUNC) {
                        if (stack[i].vector_length != vector_length) {
                            parse_error("Vector length mismatch (%d != %d).\n",
                                        stack[i].vector_length, vector_length);
                            return -1;
                        }
                    }
                    else if (stack[i].toktype == TOK_VAR
                             && stack[i].var < VAR_Y) {
                        int *vec_len = &vars[stack[i].var].vector_length;
                        if (*vec_len && *vec_len != vector_length) {
                            parse_error("Vector length mismatch (%d != %d).\n",
                                        *vec_len, vector_length);
                            return -1;
                        }
                        *vec_len = vector_length;
                        stack[i].vector_length = vector_length;
                        stack[i].vector_length_locked = 1;
                    }
                    else
                        stack[i].vector_length = vector_length;
                }
                else if (stack[i].vector_length != vector_length) {
                    parse_error("Vector length mismatch (%d != %d).\n",
                                stack[i].vector_length, vector_length);
                    return -1;
                }
            }

            switch (stack[i].toktype) {
                case TOK_OP:
                    if (skip > 0)
                        skip += op_table[stack[i].op].arity;
                    else
                        depth += op_table[stack[i].op].arity;
                    break;
                case TOK_FUNC:
                    if (skip > 0)
                        skip += function_table[stack[i].func].arity;
                    else
                        depth += function_table[stack[i].func].arity;
                    break;
                case TOK_VFUNC:
                    skip = 2;
                    break;
                case TOK_VECTORIZE:
                    skip = stack[i].arity + 1;
                    break;
                case TOK_ASSIGN_USE:
                    skip++;
                    depth++;
                    break;
                default:
                    break;
            }

            if (skip > 0)
                skip--;
            else
                depth--;
            if (depth <= 0 && skip <= 0)
                break;
        }

        if (!stack[top].vector_length_locked) {
            if (stack[top].toktype != TOK_VFUNC)
                stack[top].vector_length = vector_length;
        }
        else if (stack[top].vector_length != vector_length) {
            parse_error("Vector length mismatch (%d != %d).\n",
                        stack[top].vector_length, vector_length);
            return -1;
        }
    }
    else {
        stack[top].datatype = 'f';
    }

    // if stack within bounds of arity was only constants, we're ok to compute
    if (can_precompute)
        return top - precompute(&stack[top-arity], arity+1, vector_length);
    else
        return top;
}

static int check_assignment_types_and_lengths(mapper_token_t *stack, int top,
                                              mapper_variable_t * vars)
{
    int i = top, vector_length = 0;
    expr_var_t var = stack[top].var;

    while (i >= 0 && stack[i].toktype == TOK_ASSIGNMENT) {
        if (stack[i].var != var) {
            parse_error("Cannot mix variable references in assignment\n");
            return -1;
        }
        vector_length += stack[i].vector_length;
        i--;
    }
    if (i < 0) {
        parse_error("Malformed expression (1).");
        return -1;
    }
    if (stack[i].vector_length != vector_length) {
        parse_error("Vector length mismatch (%d != %d).\n",
                    stack[i].vector_length, vector_length);
        return -1;
    }
    promote_token_datatype(&stack[i], stack[top].datatype);
    if (check_types_and_lengths(stack, i, vars) == -1)
        return -1;
    promote_token_datatype(&stack[i], stack[top].datatype);

    if (stack[top].history_index == 0 || i == 0)
        return 0;

    // Move assignment expression to beginning of stack
    int j = 0, expr_length = top-i+1;
    if (stack[i].toktype == TOK_VECTORIZE)
        expr_length += stack[i].arity;

    mapper_token_t temp[expr_length];
    for (i = top-expr_length+1; i <= top; i++)
        memcpy(&temp[j++], &stack[i], sizeof(mapper_token_t));

    for (i = top-expr_length; i >= 0; i--)
        memcpy(&stack[i+expr_length], &stack[i], sizeof(mapper_token_t));

    for (i = 0; i < expr_length; i++)
        memcpy(&stack[i], &temp[i], sizeof(mapper_token_t));

    return 0;
}

static int find_variable_by_name(mapper_variable_t *variables, int num_variables,
                                 const char *name, int namelen)
{
    // check if variable name matches known variable
    int i;
    for (i = 0; i < num_variables; i++) {
        if (strlen(variables[i].name) == namelen
            && strncmp(variables[i].name, name, namelen)==0) {
            return i;
        }
    }
    return -1;
}

/* Macros to help express stack operations in parser. */
#define FAIL(msg) {                                                 \
    parse_error("%s\n", msg);                                       \
    while (--num_variables >= 0)                                    \
        free(variables[num_variables].name);                        \
    return 0;                                                       \
}
#define PUSH_TO_OUTPUT(x)                                           \
{                                                                   \
    if (++outstack_index >= STACK_SIZE)                             \
        {FAIL("Stack size exceeded.");}                             \
    memcpy(outstack + outstack_index, &x, sizeof(mapper_token_t));  \
}
#define PUSH_TO_OPERATOR(x)                                         \
{                                                                   \
    if (++opstack_index >= STACK_SIZE)                              \
        {FAIL("Stack size exceeded.");}                             \
    memcpy(opstack + opstack_index, &x, sizeof(mapper_token_t));    \
}
#define POP_OPERATOR() ( opstack_index-- )
#define POP_OPERATOR_TO_OUTPUT()                                    \
{                                                                   \
    PUSH_TO_OUTPUT(opstack[opstack_index]);                         \
    outstack_index = check_types_and_lengths(outstack,              \
                                             outstack_index,        \
                                             variables);            \
    if (outstack_index < 0)                                         \
         {FAIL("Malformed expression (2).");}                       \
    POP_OPERATOR();                                                 \
}
#define GET_NEXT_TOKEN(x)                                           \
{                                                                   \
    lex_index = expr_lex(str, lex_index, &x);                       \
    if (!lex_index)                                                 \
        {FAIL("Error in lexer.");}                                  \
}

/*! Use Dijkstra's shunting-yard algorithm to parse expression into RPN stack. */
mapper_expr mapper_expr_new_from_string(const char *str, int num_inputs,
                                        const char *input_types,
                                        const int *input_vector_lengths,
                                        char output_type,
                                        int output_vector_length)
{
    if (!str || !num_inputs || !input_types || !input_vector_lengths)
        return 0;

    mapper_token_t outstack[STACK_SIZE];
    mapper_token_t opstack[STACK_SIZE];
    int i, lex_index = 0, outstack_index = -1, opstack_index = -1;
    int oldest_input = 0, oldest_output = 0, max_vector = 1;

    int assigning = 0;
    int output_assigned = 0;
    int vectorizing = 0;
    int variable = 0;
    int allow_toktype = 0xFFFF;
    int constant_output = 1;
    int input_vector_length = 0;

    mapper_variable_t variables[N_USER_VARS];
    int num_variables = 0;

    int assign_mask = (TOK_VAR | TOK_OPEN_SQUARE | TOK_COMMA | TOK_CLOSE_SQUARE
                       | TOK_OPEN_CURLY);
    int OBJECT_TOKENS = (TOK_VAR | TOK_CONST | TOK_FUNC | TOK_VFUNC
                         | TOK_NEGATE | TOK_OPEN_PAREN | TOK_OPEN_SQUARE);

    mapper_token_t tok;

    // ignoring spaces at start of expression
    while (str[lex_index] == ' ') lex_index++;
    if (!str[lex_index])
        {FAIL("No expression found.");}

    assigning = 1;
    allow_toktype = TOK_VAR | TOK_OPEN_SQUARE;

#if TRACING
    printf("parsing expression '%s'\n", str);
#endif

    while (str[lex_index]) {
        GET_NEXT_TOKEN(tok);
        if (variable && tok.toktype != TOK_OPEN_SQUARE
            && tok.toktype != TOK_OPEN_CURLY)
            variable = 0;
        if (assigning) {
            if (tok.toktype != TOK_ASSIGNMENT
                && !(tok.toktype & allow_toktype & assign_mask))
                {FAIL("Illegal token sequence. (1)");}
        }
        else if (!(tok.toktype & allow_toktype)) {
            {FAIL("Illegal token sequence. (2)");}
        }
        switch (tok.toktype) {
            case TOK_CONST:
                // push to output stack
                PUSH_TO_OUTPUT(tok);
                allow_toktype = (TOK_OP | TOK_CLOSE_PAREN | TOK_CLOSE_SQUARE
                                 | TOK_COMMA | TOK_COLON | TOK_SEMICOLON);
                break;
            case TOK_VAR:
                if (tok.var >= VAR_X) {
                    int slot = tok.var-VAR_X;
                    if (slot >= num_inputs)
                        {FAIL("Input slot index exceeds number of sources.");}
                    tok.datatype = input_types[slot];
                    tok.vector_length = input_vector_lengths[slot];
                    input_vector_length = tok.vector_length;
                    tok.vector_length_locked = 1;
                    constant_output = 0;
                }
                else if (tok.var == VAR_Y) {
                    tok.datatype = output_type;
                    tok.vector_length = output_vector_length;
                    tok.vector_length_locked = 1;
                }
                else {
                    // get name of variable
                    int index = lex_index-1;
                    char c = str[index];
                    while (index >= 0 && c && (isalpha(c) || isdigit(c))) {
                        if (--index >= 0)
                            c = str[index];
                    }

                    int len = lex_index-index-1;
                    int i = find_variable_by_name(variables, num_variables,
                                                  str+index+1, len);
                    if (i >= 0) {
                        tok.var = i;
                        tok.datatype = variables[i].datatype;
                        tok.vector_length = variables[i].vector_length;
                        if (tok.vector_length)
                            tok.vector_length_locked = 1;
                    }
                    else {
                        if (num_variables >= N_USER_VARS)
                            {FAIL("Maximum number of variables exceeded.");}
                        // need to store new variable
                        variables[num_variables].name = malloc(lex_index-index);
                        snprintf(variables[num_variables].name, lex_index-index,
                                 "%s", str+index+1);
                        variables[num_variables].datatype = 'd';
                        variables[num_variables].vector_length = 0;
                        variables[num_variables].history_size = 1;
                        variables[num_variables].assigned = 0;
#if TRACING
                        printf("Stored new variable '%s' at index %i\n",
                               variables[num_variables].name, num_variables);
#endif
                        tok.var = num_variables;
                        tok.datatype = 'd';
                        tok.vector_length = 0;
                        num_variables++;
                    }
                }
                tok.history_index = 0;
                tok.vector_index = 0;
                PUSH_TO_OUTPUT(tok);
                // variables can have vector and history indices
                variable = TOK_OPEN_SQUARE | TOK_OPEN_CURLY;
                allow_toktype = (TOK_OP | TOK_CLOSE_PAREN | TOK_CLOSE_SQUARE
                                 | TOK_COMMA | TOK_COLON | TOK_SEMICOLON
                                 | variable | (assigning ? TOK_ASSIGNMENT : 0));
                break;
            case TOK_FUNC:
                if (function_table[tok.func].func_int32)
                    tok.datatype = 'i';
                else
                    tok.datatype = 'f';
                mapper_token_t newtok;
                if (function_table[tok.func].memory) {
                    // add assignment token
                    if (num_variables >= N_USER_VARS)
                        {FAIL("Maximum number of variables exceeded.");}
                    char varname[6];
                    int varindex = num_variables;
                    do {
                        snprintf(varname, 6, "var%d", varindex++);
                    } while (find_variable_by_name(variables, num_variables,
                                                   varname, 6) >= 0);
                    // need to store new variable
                    variables[num_variables].name = strdup(varname);
                    variables[num_variables].datatype = 'd';
                    variables[num_variables].vector_length = 1;
                    variables[num_variables].history_size = 1;
                    variables[num_variables].assigned = 1;

                    newtok.toktype = TOK_ASSIGN_USE;
                    newtok.var = num_variables;
                    num_variables++;
                    newtok.datatype = 'd';
                    newtok.vector_length = 1;
                    newtok.vector_length_locked = 0;
                    newtok.history_index = 0;
                    newtok.vector_index = 0;
                    newtok.assignment_offset = 0;
                    PUSH_TO_OPERATOR(newtok);
                }
                PUSH_TO_OPERATOR(tok);
                if (!function_table[tok.func].arity) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (function_table[tok.func].arity)
                    allow_toktype = TOK_OPEN_PAREN;
                else
                    allow_toktype = (TOK_OP | TOK_CLOSE_PAREN | TOK_CLOSE_SQUARE
                                     | TOK_COMMA | TOK_COLON | TOK_SEMICOLON);
                if (tok.func >= FUNC_UNIFORM)
                    constant_output = 0;
                if (function_table[tok.func].memory) {
                    newtok.toktype = TOK_VAR;
                    newtok.history_index = -1;
                    PUSH_TO_OUTPUT(newtok);
                }
                break;
            case TOK_VFUNC:
                if (vfunction_table[tok.func].func_int32)
                    tok.datatype = 'i';
                else
                    tok.datatype = 'f';
                PUSH_TO_OPERATOR(tok);
                allow_toktype = TOK_OPEN_PAREN;
                break;
            case TOK_OPEN_PAREN:
                tok.arity = 1;
                PUSH_TO_OPERATOR(tok);
                allow_toktype = OBJECT_TOKENS;
                break;
            case TOK_CLOSE_PAREN:
                // pop from operator stack to output until left parenthesis found
                while (opstack_index >= 0 && opstack[opstack_index].toktype != TOK_OPEN_PAREN) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched parentheses or misplaced comma.");}

                int arity = opstack[opstack_index].arity;
                // remove left parenthesis from operator stack
                POP_OPERATOR();

                // if stack[top] is tok_func or tok_vfunc, pop to output
                if (opstack[opstack_index].toktype == TOK_FUNC) {
                    // check for overloaded functions
                    if (arity == 1) {
                        if (opstack[opstack_index].func == FUNC_MIN) {
                            opstack[opstack_index].toktype = TOK_VFUNC;
                            opstack[opstack_index].vfunc = VFUNC_MIN;
                        }
                        else if (opstack[opstack_index].func == FUNC_MAX) {
                            opstack[opstack_index].toktype = TOK_VFUNC;
                            opstack[opstack_index].vfunc = VFUNC_MAX;
                        }
                    }
                    POP_OPERATOR_TO_OUTPUT();
                }
                else if (opstack[opstack_index].toktype == TOK_VFUNC)
                    POP_OPERATOR_TO_OUTPUT();
                // special case: if stack[top] is tok_assignment, pop to output
                if (opstack[opstack_index].toktype == TOK_ASSIGN_USE)
                    POP_OPERATOR_TO_OUTPUT();
                allow_toktype = (TOK_OP | TOK_COLON | TOK_SEMICOLON | TOK_COMMA
                                 | TOK_CLOSE_PAREN | TOK_CLOSE_SQUARE);
                break;
            case TOK_COMMA:
                // pop from operator stack to output until left parenthesis or TOK_VECTORIZE found
                while (opstack_index >= 0
                       && opstack[opstack_index].toktype != TOK_OPEN_PAREN
                       && opstack[opstack_index].toktype != TOK_VECTORIZE) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0) {
                    {FAIL("Malformed expression (3).");}
                    break;
                }
                if (opstack[opstack_index].toktype == TOK_VECTORIZE) {
                    opstack[opstack_index].vector_index++;
                    opstack[opstack_index].vector_length +=
                        outstack[outstack_index].vector_length;
                    lock_vector_lengths(outstack, outstack_index);
                }
                else {
                    // open paren
                    opstack[opstack_index].arity++;
                }
                allow_toktype = OBJECT_TOKENS;
                break;
            case TOK_COLON:
                // pop from operator stack to output until conditional found
                while (opstack_index >= 0 &&
                       (opstack[opstack_index].toktype != TOK_OP ||
                        opstack[opstack_index].op != OP_CONDITIONAL_IF_THEN)) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched colon.");}
                opstack[opstack_index].op = OP_CONDITIONAL_IF_THEN_ELSE;
                allow_toktype = OBJECT_TOKENS;
                break;
            case TOK_SEMICOLON:
                while (opstack_index >= 0) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index > 0)
                    {FAIL("Malformed expression (4).");}
                // check if last expression was assigned correctly
                if (outstack[outstack_index].toktype < TOK_ASSIGNMENT)
                    {FAIL("Malformed expression (5).");}
                if (check_assignment_types_and_lengths(outstack, outstack_index,
                                                       variables) == -1)
                    {FAIL("Malformed expression (6).");}
                int var_idx = outstack[outstack_index].var;
                if (var_idx < VAR_Y) {
                    if (!variables[var_idx].vector_length)
                        variables[var_idx].vector_length
                            = outstack[outstack_index].vector_length;
                    // lock vector length of assigned variable
                    outstack[outstack_index].vector_length_locked = 1;
                }
                // start another sub-expression
                assigning = 1;
                allow_toktype = TOK_VAR;
                break;
            case TOK_OP:
                // check precedence of operators on stack
                while (opstack_index >= 0 && opstack[opstack_index].toktype == TOK_OP
                       && op_table[opstack[opstack_index].op].precedence >=
                       op_table[tok.op].precedence) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                PUSH_TO_OPERATOR(tok);
                allow_toktype = OBJECT_TOKENS;
                break;
            case TOK_OPEN_SQUARE:
                if (variable & TOK_OPEN_SQUARE) { // vector index not set
                    GET_NEXT_TOKEN(tok);
                    if (tok.toktype != TOK_CONST || tok.datatype != 'i')
                        {FAIL("Non-integer vector index.");}
                    if (outstack[outstack_index].var == VAR_Y) {
                        if (tok.i >= output_vector_length)
                            {FAIL("Index exceeds output vector length.");}
                    }
                    else if (tok.i >= input_vector_length)
                        {FAIL("Index exceeds input vector length.");}
                    outstack[outstack_index].vector_index = tok.i;
                    outstack[outstack_index].vector_length = 1;
                    outstack[outstack_index].vector_length_locked = 1;
                    GET_NEXT_TOKEN(tok);
                    if (tok.toktype == TOK_COLON) {
                        // index is range A:B
                        GET_NEXT_TOKEN(tok);
                        if (tok.toktype != TOK_CONST || tok.datatype != 'i')
                            {FAIL("Malformed vector index.");}
                        if (outstack[outstack_index].var == VAR_Y) {
                            if (tok.i >= output_vector_length)
                                {FAIL("Index exceeds output vector length.");}
                        }
                        else if (tok.i >= input_vector_length)
                            {FAIL("Index exceeds input vector length.");}
                        if (tok.i <= outstack[outstack_index].vector_index)
                            {FAIL("Malformed vector index.");}
                        outstack[outstack_index].vector_length =
                            tok.i - outstack[outstack_index].vector_index + 1;
                        GET_NEXT_TOKEN(tok);
                    }
                    if (tok.toktype != TOK_CLOSE_SQUARE)
                        {FAIL("Unmatched bracket.");}
                    // vector index set
                    variable &= ~TOK_OPEN_SQUARE;
                    allow_toktype = (TOK_OP | TOK_COMMA | TOK_CLOSE_PAREN
                                     | TOK_CLOSE_SQUARE | TOK_COLON | TOK_SEMICOLON
                                     | variable | (assigning ? TOK_ASSIGNMENT : 0));
                }
                else {
                    if (vectorizing)
                        {FAIL("Nested (multidimensional) vectors not allowed.");}
                    tok.toktype = TOK_VECTORIZE;
                    tok.vector_length = 0;
                    tok.arity = 0;
                    PUSH_TO_OPERATOR(tok);
                    vectorizing = 1;
                    allow_toktype = OBJECT_TOKENS & ~TOK_OPEN_SQUARE;
                }
                break;
            case TOK_CLOSE_SQUARE:
                // pop from operator stack to output until TOK_VECTORIZE found
                while (opstack_index >= 0 &&
                       opstack[opstack_index].toktype != TOK_VECTORIZE) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched brackets or misplaced comma.");}
                if (opstack[opstack_index].vector_length) {
                    opstack[opstack_index].vector_length_locked = 1;
                    opstack[opstack_index].arity++;
                    opstack[opstack_index].vector_length +=
                        outstack[outstack_index].vector_length;
                    lock_vector_lengths(outstack, outstack_index);
                    POP_OPERATOR_TO_OUTPUT();
                }
                else {
                    // we do not need vectorizer token if vector length == 1
                    POP_OPERATOR();
                }
                vectorizing = 0;
                allow_toktype = (TOK_OP | TOK_CLOSE_PAREN | TOK_COMMA
                                 | TOK_COLON | TOK_SEMICOLON);
                break;
            case TOK_OPEN_CURLY:
                if (!(variable & TOK_OPEN_CURLY)) // history index already set
                    {FAIL("Misplaced brace.");}
                GET_NEXT_TOKEN(tok);
                if (tok.toktype == TOK_NEGATE) {
                    // if negative sign found, get next token
                    outstack[outstack_index].history_index = -1;
                    GET_NEXT_TOKEN(tok);
                }
                if (tok.toktype != TOK_CONST || tok.datatype != 'i')
                    {FAIL("Non-integer history index.");}
                outstack[outstack_index].history_index *= tok.i;
                if (outstack[outstack_index].var == VAR_Y) {
                    if (outstack[outstack_index].history_index > -1)
                        {FAIL("Output history index cannot be > -1.");}
                    else if (outstack[outstack_index].history_index < MAX_HISTORY)
                        {FAIL("Output history index cannot be < -100.");}
                }
                else {
                    if (outstack[outstack_index].history_index > 0)
                    {FAIL("Input history index cannot be > 0.");}
                    else if (outstack[outstack_index].history_index < MAX_HISTORY)
                    {FAIL("Input history index cannot be < -100.");}
                }
                if (outstack[outstack_index].var == VAR_X) {
                    if (outstack[outstack_index].history_index < oldest_input)
                        oldest_input = outstack[outstack_index].history_index;
                }
                else if (outstack[outstack_index].var == VAR_Y) {
                    if (outstack[outstack_index].history_index < oldest_output)
                        oldest_output = outstack[outstack_index].history_index;
                }
                else {
                    // user-defined variable
                    int var_idx = outstack[outstack_index].var;
                    int hist_idx = outstack[outstack_index].history_index;
                    if ((hist_idx * -1 + 1) > variables[var_idx].history_size)
                        variables[var_idx].history_size = hist_idx * -1 + 1;
                }
                GET_NEXT_TOKEN(tok);
                if (tok.toktype != TOK_CLOSE_CURLY)
                    {FAIL("Unmatched brace.");}
                variable &= ~TOK_OPEN_CURLY;
                allow_toktype = (TOK_OP | TOK_COMMA | TOK_CLOSE_PAREN
                                 | TOK_CLOSE_SQUARE | TOK_COLON | TOK_SEMICOLON
                                 | variable | (assigning ? TOK_ASSIGNMENT : 0));
                break;
            case TOK_NEGATE:
                // push '-1' to output stack, and '*' to operator stack
                tok.toktype = TOK_CONST;
                tok.datatype = 'i';
                tok.i = -1;
                PUSH_TO_OUTPUT(tok);
                tok.toktype = TOK_OP;
                tok.op = OP_MULTIPLY;
                PUSH_TO_OPERATOR(tok);
                allow_toktype = TOK_CONST | TOK_VAR | TOK_FUNC | TOK_VFUNC;
                break;
            case TOK_ASSIGNMENT:
                // assignment to variable
                if (!assigning)
                    {FAIL("Misplaced assignment operator.");}
                if (opstack_index >= 0 || outstack_index < 0)
                    {FAIL("Malformed expression left of assignment.");}

                if (outstack[outstack_index].toktype == TOK_VAR) {
                    int var = outstack[outstack_index].var;
                    if (var >= VAR_X)
                        {FAIL("Cannot assign to input variable 'x'.");}
                    else if (var == VAR_Y) {
                        if (outstack[outstack_index].history_index == 0) {
                            if (output_assigned)
                                {FAIL("Variable 'y' already assigned.");}
                            output_assigned = 1;
                        }
                    }
                    else if (outstack[outstack_index].history_index == 0) {
                        variables[var].assigned = 1;
                    }
                    // nothing extraordinary, continue as normal
                    outstack[outstack_index].toktype = TOK_ASSIGNMENT;
                    outstack[outstack_index].assignment_offset = 0;
                    PUSH_TO_OPERATOR(outstack[outstack_index]);
                    outstack_index--;
                }
                else if (outstack[outstack_index].toktype == TOK_VECTORIZE) {
                    // outstack token is vectorizer
                    outstack_index--;
                    if (outstack[outstack_index].toktype != TOK_VAR)
                        {FAIL("Illegal tokens left of assignment.");}
                    int var = outstack[outstack_index].var;
                    if (var >= VAR_X)
                        {FAIL("Cannot assign to input variable 'x'.");}
                    else if (var == VAR_Y) {
                        if (outstack[outstack_index].history_index == 0) {
                            if (output_assigned)
                                {FAIL("Variable 'y' already assigned.");}
                            output_assigned = 1;
                        }
                    }
                    else if (outstack[outstack_index].history_index == 0) {
                        variables[var].assigned = 1;
                    }
                    while (outstack_index >= 0) {
                        if (outstack[outstack_index].toktype != TOK_VAR)
                            {FAIL("Illegal tokens left of assignment.");}
                        else if (outstack[outstack_index].var != var)
                            {FAIL("Cannot mix variables in vector assignment.");}
                        outstack[outstack_index].toktype = TOK_ASSIGNMENT;
                        PUSH_TO_OPERATOR(outstack[outstack_index]);
                        outstack_index--;
                    }
                    int index = opstack_index, vector_count = 0;
                    while (index >= 0) {
                        opstack[index].assignment_offset = vector_count;
                        vector_count += opstack[index].vector_length;
                        index--;
                    }
                }
                else
                    {FAIL("Malformed expression left of assignment.");}
                assigning = 0;
                allow_toktype = 0xFFFF;
                break;
            default:
                {FAIL("Unknown token type.");}
                break;
        }
#if (TRACING && DEBUG)
        printstack("OUTPUT STACK:", outstack, outstack_index);
        printstack("OPERATOR STACK:", opstack, opstack_index);
#endif
    }

    if (allow_toktype & TOK_CONST || !output_assigned)
        {FAIL("Expression has no output assignment.");}

    // check that all used-defined variables were assigned
    for (i = 0; i < num_variables; i++) {
        if (!variables[i].assigned)
            {FAIL("User-defined variable not assigned.");}
    }

    // finish popping operators to output, check for unbalanced parentheses
    while (opstack_index >= 0 && opstack[opstack_index].toktype < TOK_ASSIGNMENT) {
        if (opstack[opstack_index].toktype == TOK_OPEN_PAREN)
            {FAIL("Unmatched parentheses or misplaced comma.");}
        POP_OPERATOR_TO_OUTPUT();
    }

    // pop assignment operator(s) to output
    while (opstack_index >= 0) {
        if (!opstack_index && opstack[opstack_index].toktype < TOK_ASSIGNMENT)
            {FAIL("Malformed expression (7).");}
        PUSH_TO_OUTPUT(opstack[opstack_index]);
        if (outstack[outstack_index].toktype == TOK_ASSIGN_USE
            && check_assignment_types_and_lengths(outstack, outstack_index,
                                                  variables) == -1) {
            // check vector length and type
            {FAIL("Malformed expression (8).");}
        }
        POP_OPERATOR();
    }

    // check vector length and type
    if (check_assignment_types_and_lengths(outstack, outstack_index,
                                           variables) == -1)
        {FAIL("Malformed expression (9).");}

#if (TRACING && DEBUG)
    printstack("--->OUTPUT STACK:", outstack, outstack_index);
    printstack("--->OPERATOR STACK:", opstack, opstack_index);
#endif

    // Check for maximum vector length used in stack
    for (i = 0; i < outstack_index; i++) {
        if (outstack[i].vector_length > max_vector)
            max_vector = outstack[i].vector_length;
    }

    mapper_expr expr = malloc(sizeof(struct _mapper_expr));
    expr->length = outstack_index + 1;
    expr->start_offset = 0;

    // copy tokens
    expr->tokens = malloc(sizeof(struct _token)*expr->length);
    memcpy(expr->tokens, &outstack, sizeof(struct _token)*expr->length);
    expr->start = expr->tokens;
    expr->vector_size = max_vector;
    expr->output_history_size = -oldest_output+1;
    expr->constant_output = constant_output;

    if (num_variables) {
        // copy user-defined variables
        expr->variables = malloc(sizeof(mapper_variable_t)*num_variables);
        memcpy(expr->variables, variables, sizeof(mapper_variable_t)*num_variables);
    }
    expr->num_variables = num_variables;

    return expr;
}

int mapper_expr_input_history_size(mapper_expr expr, int index)
{
    int i, size = 0, var = index + VAR_X;
    mapper_token_t *tok = expr->tokens;
    for (i = 0; i < expr->length; i++) {
        if (tok[i].toktype == TOK_VAR && tok[i].var == var) {
            if (tok[i].history_index < size)
                size = tok[i].history_index;
        }
    }
    return -size + 1;
}

int mapper_expr_output_history_size(mapper_expr expr)
{
    return expr->output_history_size;
}

int mapper_expr_num_variables(mapper_expr expr)
{
    return expr->num_variables;
}

int mapper_expr_variable_history_size(mapper_expr expr, int index)
{
    if (index >= expr->num_variables)
        return 0;
    else
        return expr->variables[index].history_size;
}

int mapper_expr_variable_vector_length(mapper_expr expr, int index)
{
    if (index >= expr->num_variables)
        return 0;
    else
        return expr->variables[index].vector_length;
}

int mapper_expr_constant_output(mapper_expr expr)
{
    if (expr->constant_output)
        return 1;
    return 0;
}

int mapper_expr_num_input_slots(mapper_expr expr)
{
    // actually need to return highest numbered input slot
    int i, count = VAR_X;
    mapper_token_t *tok = expr->tokens;
    for (i = 0; i < expr->length; i++) {
        if (tok[i].toktype == TOK_VAR && tok[i].var > count) {
            count = tok[i].var;
        }
    }
    return count - VAR_X;
}

#if TRACING
static void print_stack_vector(mapper_value_t *stack, char type,
                               int vector_length)
{
    int i;
    if (vector_length > 1)
        printf("[");
    switch (type) {
        case 'i':
            for (i = 0; i < vector_length; i++)
                printf("%d, ", stack[i].i32);
            break;
        case 'f':
            for (i = 0; i < vector_length; i++)
                printf("%f, ", stack[i].f);
            break;
        case 'd':
            for (i = 0; i < vector_length; i++)
                printf("%f, ", stack[i].d);
            break;
        default:
            break;
    }
    if (vector_length > 1)
        printf("\b\b]");
    else
        printf("\b\b");
}
#endif

#if TRACING
static const char *type_name(const char type)
{
    switch (type) {
        case 'd':
            return "double";
        case 'f':
            return "float";
        case 'i':
            return "int";
        default:
            return 0;
    }
}
#endif

int mapper_expr_evaluate(mapper_expr expr, mapper_history *input,
                         mapper_history *expr_vars, mapper_history output,
                         mapper_timetag_t *tt, char *typestring)
{
    if (!expr) {
#if TRACING
        printf(" no expression to evaluate!\n");
#endif
        return 0;
    }
    mapper_token_t *tok = expr->start;
    int length = expr->length;
    if (output->position >= 0) {
        tok += expr->start_offset;
        length -= expr->start_offset;
    }
    mapper_value_t stack[length][expr->vector_size];
    int dims[length];

    int i, j, k, top = -1, count = 0, found, updated = 0;

    // init typestring
    if (typestring)
        memset(typestring, 'N', output->length);

    /* Increment index position of output data structure. */
    output->position = (output->position + 1) % output->size;

    for (i = 0; i < expr->num_variables; i++)
        expr->variables[i].assigned = 0;

    while (count < length && tok->toktype != TOK_END) {
        switch (tok->toktype) {
        case TOK_CONST:
            ++top;
            dims[top] = tok->vector_length;
#if TRACING
            if (tok->datatype == 'f')
                printf("storing const %f\n", tok->f);
            else if (tok->datatype == 'i')
                printf("storing const %i\n", tok->i);
            else if (tok->datatype == 'd')
                printf("storing const %f\n", tok->d);
#endif
            switch (tok->datatype) {
            case 'i':
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].i32 = tok->i;
                break;
            case 'f':
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].f = tok->f;
                break;
            case 'd':
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].d = tok->d;
                break;
            default:
                goto error;
            }
            break;
        case TOK_VAR: {
            int idx;
            if (tok->var == VAR_Y) {
                ++top;
                dims[top] = tok->vector_length;
                idx = ((tok->history_index + output->position
                        + output->size) % output->size);
                switch (output->type) {
                case 'f': {
                    float *v = (output->value + idx * output->length
                                * mapper_type_size(output->type));
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = v[i+tok->vector_index];
                    break;
                }
                case 'i': {
                    int *v = (output->value + idx * output->length
                              * mapper_type_size(output->type));
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = v[i+tok->vector_index];
                    break;
                }
                case 'd': {
                    double *v = (output->value + idx * output->length
                                 * mapper_type_size(output->type));
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = v[i+tok->vector_index];
                    break;
                }
                default:
                    goto error;
                }
#if TRACING
                printf("loading variable y{%d}[%d] ", tok->history_index,
                       tok->vector_index);
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" \n");
#endif
            }
            else if (tok->var >= VAR_X) {
                ++top;
                dims[top] = tok->vector_length;
                mapper_history h = input[tok->var-VAR_X];
                idx = ((tok->history_index + h->position + h->size) % h->size);
                switch (h->type) {
                case 'f': {
                    float *v = (h->value + idx * h->length
                                * mapper_type_size(h->type));
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = v[i+tok->vector_index];
                    break;
                }
                case 'i': {
                    int *v = (h->value + idx * h->length
                              * mapper_type_size(h->type));
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = v[i+tok->vector_index];
                    break;
                }
                case 'd': {
                    double *v = (h->value + idx * h->length
                                 * mapper_type_size(h->type));
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = v[i+tok->vector_index];
                    break;
                }
                default:
                    goto error;
                }
#if TRACING
                printf("loading variable x%d{%d}[%d] ", tok->var-VAR_X,
                       tok->history_index, tok->vector_index);
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" \n");
#endif
            }
            else if (expr_vars) {
                // TODO: allow other data types?
                ++top;
                dims[top] = tok->vector_length;
                mapper_variable var = &expr->variables[tok->var];
                mapper_history h = *expr_vars + (tok->var);
                idx = ((tok->history_index + h->position
                        + var->history_size) % var->history_size);
                double *v = h->value + idx * var->vector_length * mapper_type_size(var->datatype);
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].d = v[i+tok->vector_index];
#if TRACING
                printf("loading variable %s{%d}[%d] ",
                       expr->variables[tok->var].name, tok->history_index,
                       tok->vector_index);
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" \n");
#endif
            }
            else
                goto error;
            break;
        }
        case TOK_OP:
            top -= op_table[tok->op].arity-1;
            dims[top] = tok->vector_length;
#if TRACING
            if (tok->op == OP_CONDITIONAL_IF_THEN || tok->op == OP_CONDITIONAL_IF_ELSE ||
                tok->op == OP_CONDITIONAL_IF_THEN_ELSE) {
                printf("IF ");
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" THEN ");
                if (tok->op == OP_CONDITIONAL_IF_ELSE) {
                    print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                    printf(" ELSE ");
                    print_stack_vector(stack[top+1], tok->datatype, tok->vector_length);
                }
                else {
                    print_stack_vector(stack[top+1], tok->datatype, tok->vector_length);
                    if (tok->op == OP_CONDITIONAL_IF_THEN_ELSE) {
                        printf(" ELSE ");
                        print_stack_vector(stack[top+2], tok->datatype, tok->vector_length);
                    }
                }
            }
            else {
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" %s%c ", op_table[tok->op].name, tok->datatype);
                print_stack_vector(stack[top+1], tok->datatype, tok->vector_length);
            }
#endif
            switch (tok->datatype) {
            case 'f': {
                switch (tok->op) {
                case OP_ADD:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f + stack[top+1][i].f;
                    break;
                case OP_SUBTRACT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f - stack[top+1][i].f;
                    break;
                case OP_MULTIPLY:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f * stack[top+1][i].f;
                    break;
                case OP_DIVIDE:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f / stack[top+1][i].f;
                    break;
                case OP_MODULO:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = fmod(stack[top][i].f, stack[top+1][i].f);
                    break;
                case OP_IS_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f == stack[top+1][i].f;
                    break;
                case OP_IS_NOT_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f != stack[top+1][i].f;
                    break;
                case OP_IS_LESS_THAN:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f < stack[top+1][i].f;
                    break;
                case OP_IS_LESS_THAN_OR_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f <= stack[top+1][i].f;
                    break;
                case OP_IS_GREATER_THAN:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f > stack[top+1][i].f;
                    break;
                case OP_IS_GREATER_THAN_OR_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f >= stack[top+1][i].f;
                    break;
                case OP_LOGICAL_AND:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f && stack[top+1][i].f;
                    break;
                case OP_LOGICAL_OR:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = stack[top][i].f || stack[top+1][i].f;
                    break;
                case OP_LOGICAL_NOT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = !stack[top][i].f;
                    break;
                case OP_CONDITIONAL_IF_THEN:
                    // TODO: should not permit implicit any()/all()
                    for (i = 0; i < tok->vector_length; i++) {
                        if (stack[top][i].f)
                            stack[top][i].f = stack[top+1][i].f;
                        else {
                            // skip ahead until after assignment
                            found = 0;
                            tok++;
                            count++;
                            while (count < length && tok->toktype != TOK_END) {
                                if (tok->toktype == TOK_ASSIGNMENT)
                                    found = 1;
                                else if (found) {
                                    --tok;
                                    --count;
                                    break;
                                }
                                tok++;
                                count++;
                            }
                        }
                    }
                    break;
                case OP_CONDITIONAL_IF_ELSE:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (!stack[top][i].f)
                            stack[top][i].f = stack[top+1][i].f;
                    }
                    break;
                case OP_CONDITIONAL_IF_THEN_ELSE:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (stack[top][i].f)
                            stack[top][i].f = stack[top+1][i].f;
                        else
                            stack[top][i].f = stack[top+2][i].f;
                    }
                    break;
                default: goto error;
                }
                break;
            }
            case 'i': {
            switch (tok->op) {
                case OP_ADD:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 + stack[top+1][i].i32;
                    break;
                case OP_SUBTRACT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 - stack[top+1][i].i32;
                    break;
                case OP_MULTIPLY:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 * stack[top+1][i].i32;
                    break;
                case OP_DIVIDE:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 / stack[top+1][i].i32;
                    break;
                case OP_MODULO:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 % stack[top+1][i].i32;
                    break;
                case OP_IS_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 == stack[top+1][i].i32;
                    break;
                case OP_IS_NOT_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 != stack[top+1][i].i32;
                    break;
                case OP_IS_LESS_THAN:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 < stack[top+1][i].i32;
                    break;
                case OP_IS_LESS_THAN_OR_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 <= stack[top+1][i].i32;
                    break;
                case OP_IS_GREATER_THAN:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 > stack[top+1][i].i32;
                    break;
                case OP_IS_GREATER_THAN_OR_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 >= stack[top+1][i].i32;
                    break;
                case OP_LEFT_BIT_SHIFT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 << stack[top+1][i].i32;
                    break;
                case OP_RIGHT_BIT_SHIFT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 >> stack[top+1][i].i32;
                    break;
                case OP_BITWISE_AND:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 & stack[top+1][i].i32;
                    break;
                case OP_BITWISE_OR:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 | stack[top+1][i].i32;
                    break;
                case OP_BITWISE_XOR:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 ^ stack[top+1][i].i32;
                    break;
                case OP_LOGICAL_AND:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 && stack[top+1][i].i32;
                    break;
                case OP_LOGICAL_OR:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = stack[top][i].i32 || stack[top+1][i].i32;
                    break;
                case OP_LOGICAL_NOT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = !stack[top][i].i32;
                    break;
                case OP_CONDITIONAL_IF_THEN:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (stack[top][i].i32)
                            stack[top][i].i32 = stack[top+1][i].i32;
                        else {
                            // skip ahead until after assignment
                            found = 0; // found assignment
                            tok++;
                            count++;
                            while (count < length && tok->toktype != TOK_END) {
                                if (tok->toktype == TOK_ASSIGNMENT)
                                    found = 1;
                                else if (found) {
                                    --tok;
                                    --count;
                                    break;
                                }
                                tok++;
                                count++;
                            }
                        }
                    }
                    break;
                case OP_CONDITIONAL_IF_ELSE:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (!stack[top][i].i32)
                            stack[top][i].i32 = stack[top+1][i].i32;
                    }
                    break;
                case OP_CONDITIONAL_IF_THEN_ELSE:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (stack[top][i].i32)
                            stack[top][i].i32 = stack[top+1][i].i32;
                        else
                            stack[top][i].i32 = stack[top+2][i].i32;
                    }
                    break;
                default: goto error;
                }
                break;
            }
            case 'd': {
            switch (tok->op) {
                case OP_ADD:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d + stack[top+1][i].d;
                    break;
                case OP_SUBTRACT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d - stack[top+1][i].d;
                    break;
                case OP_MULTIPLY:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d * stack[top+1][i].d;
                    break;
                case OP_DIVIDE:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d / stack[top+1][i].d;
                    break;
                case OP_MODULO:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = fmod(stack[top][i].d, stack[top+1][i].d);
                    break;
                case OP_IS_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d == stack[top+1][i].d;
                    break;
                case OP_IS_NOT_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d != stack[top+1][i].d;
                    break;
                case OP_IS_LESS_THAN:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d < stack[top+1][i].d;
                    break;
                case OP_IS_LESS_THAN_OR_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d <= stack[top+1][i].d;
                    break;
                case OP_IS_GREATER_THAN:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d > stack[top+1][i].d;
                    break;
                case OP_IS_GREATER_THAN_OR_EQUAL:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d >= stack[top+1][i].d;
                    break;
                case OP_LOGICAL_AND:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d && stack[top+1][i].d;
                    break;
                case OP_LOGICAL_OR:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = stack[top][i].d || stack[top+1][i].d;
                    break;
                case OP_LOGICAL_NOT:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = !stack[top][i].d;
                    break;
                case OP_CONDITIONAL_IF_THEN:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (stack[top][i].d)
                            stack[top][i].d = stack[top+1][i].d;
                        else {
                            // skip ahead until after assignment
                            found = 0; // found assignment
                            tok++;
                            count++;
                            while (count < length && tok->toktype != TOK_END) {
                                if (tok->toktype == TOK_ASSIGNMENT)
                                    found = 1;
                                else if (found) {
                                    --tok;
                                    --count;
                                    break;
                                }
                                tok++;
                                count++;
                            }
                        }
                    }
                    break;
                case OP_CONDITIONAL_IF_ELSE:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (!stack[top][i].d)
                            stack[top][i].d = stack[top+1][i].d;
                    }
                    break;
                case OP_CONDITIONAL_IF_THEN_ELSE:
                    for (i = 0; i < tok->vector_length; i++) {
                        if (stack[top][i].d)
                            stack[top][i].d = stack[top+1][i].d;
                        else
                            stack[top][i].d = stack[top+2][i].d;
                    }
                    break;
                default: goto error;
                }
                break;
            }
            default:
                goto error;
            }
#if TRACING
            printf(" = ");
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        case TOK_FUNC:
            top -= function_table[tok->func].arity-1;
            dims[top] = tok->vector_length;
#if TRACING
            printf("%s%c(", function_table[tok->func].name, tok->datatype);
            for (i = 0; i < function_table[tok->func].arity; i++) {
                print_stack_vector(stack[top+i], tok->datatype, tok->vector_length);
                printf(", ");
            }
            printf("%s)", function_table[tok->func].arity ? "\b\b" : "");
#endif
            switch (tok->datatype) {
            case 'f':
                switch (function_table[tok->func].arity) {
                case 0:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = ((func_float_arity0*)
                                           function_table[tok->func].func_float)();
                    break;
                case 1:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = (((func_float_arity1*)
                                            function_table[tok->func].func_float)
                                           (stack[top][i].f));
                    break;
                case 2:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = (((func_float_arity2*)
                                            function_table[tok->func].func_float)
                                           (stack[top][i].f, stack[top+1][i].f));
                    break;
                case 3:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = (((func_float_arity3*)
                                            function_table[tok->func].func_float)
                                           (stack[top][i].f, stack[top+1][i].f,
                                            stack[top+2][i].f));
                    break;
                case 4:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].f = (((func_float_arity4*)
                                            function_table[tok->func].func_float)
                                           (stack[top][i].f, stack[top+1][i].f,
                                            stack[top+2][i].f, stack[top+3][i].f));
                    break;
                default: goto error;
                }
                break;
            case 'i':
                switch (function_table[tok->func].arity) {
                case 0:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = ((func_int32_arity0*)
                                             function_table[tok->func].func_int32)();
                    break;
                case 1:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = (((func_int32_arity1*)
                                              function_table[tok->func].func_int32)
                                             (stack[top][i].i32));
                    break;
                case 2:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].i32 = (((func_int32_arity2*)
                                              function_table[tok->func].func_int32)
                                             (stack[top][i].i32, stack[top+1][i].i32));
                    break;
                default: goto error;
                }
                break;
            case 'd':
                switch (function_table[tok->func].arity) {
                case 0:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = ((func_double_arity0*)
                                           function_table[tok->func].func_double)();
                    break;
                case 1:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = (((func_double_arity1*)
                                            function_table[tok->func].func_double)
                                           (stack[top][i].d));
                    break;
                case 2:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = (((func_double_arity2*)
                                            function_table[tok->func].func_double)
                                           (stack[top][i].d, stack[top+1][i].d));
                    break;
                case 3:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = (((func_double_arity3*)
                                            function_table[tok->func].func_double)
                                           (stack[top][i].d, stack[top+1][i].d,
                                            stack[top+2][i].d));
                    break;
                case 4:
                    for (i = 0; i < tok->vector_length; i++)
                        stack[top][i].d = (((func_double_arity4*)
                                            function_table[tok->func].func_double)
                                           (stack[top][i].d, stack[top+1][i].d,
                                            stack[top+2][i].d, stack[top+3][i].d));
                    break;
                default: goto error;
                }
                break;
            default:
                goto error;
            }
#if TRACING
            printf(" = ");
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        case TOK_VFUNC:
            top -= vfunction_table[tok->func].arity-1;
#if TRACING
            printf("%s%c(", vfunction_table[tok->func].name, tok->datatype);
            for (i = 0; i < vfunction_table[tok->func].arity; i++) {
                print_stack_vector(stack[top], tok->datatype, dims[top]);
                printf(", ");
            }
            printf("\b\b)");
#endif
            switch (tok->datatype) {
            case 'f':
                switch (vfunction_table[tok->func].arity) {
                    case 1:
                        stack[top][0].f = ((vfunc_float_arity1*)vfunction_table[tok->func].func_float)(stack[top], dims[top]);
                        for (i = 1; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][0].f;
                        break;
                    default: goto error;
                }
                break;
            case 'i':
                switch (vfunction_table[tok->func].arity) {
                    case 1:
                        stack[top][0].i32 = ((vfunc_int32_arity1*)vfunction_table[tok->func].func_int32)(stack[top], dims[top]);
                        for (i = 1; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][0].i32;
                        break;
                    default: goto error;
                }
                break;
            case 'd':
                switch (vfunction_table[tok->func].arity) {
                    case 1:
                        stack[top][0].d = ((vfunc_double_arity1*)vfunction_table[tok->func].func_double)(stack[top], dims[top]);
                        for (i = 1; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][0].d;
                        break;
                    default: goto error;
                }
                break;
            default:
                break;
            }
            dims[top] = tok->vector_length;
#if TRACING
            printf(" = ");
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        case TOK_VECTORIZE:
            // don't need to copy vector elements from first token
            top -= tok->arity-1;
            k = dims[top];
            switch (tok->datatype) {
            case 'f':
                for (i = 1; i < tok->arity; i++) {
                    for (j = 0; j < dims[top+1]; j++)
                        stack[top][k++].f = stack[top+i][j].f;
                }
                break;
            case 'i':
                for (i = 1; i < tok->arity; i++) {
                    for (j = 0; j < dims[top+1]; j++)
                        stack[top][k++].i32 = stack[top+i][j].i32;
                }
                break;
            case 'd':
                for (i = 1; i < tok->arity; i++) {
                    for (j = 0; j < dims[top+1]; j++)
                        stack[top][k++].d = stack[top+i][j].d;
                }
                break;
            default:
                goto error;
            }
            dims[top] = tok->vector_length;
#if TRACING
            printf("built %i-element vector: ", tok->vector_length);
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        case TOK_ASSIGNMENT:
        case TOK_ASSIGN_USE:
#if TRACING
            if (tok->var == VAR_Y)
                printf("assigning values to y{%i}[%i] (%s x %d)\n",
                       tok->history_index, tok->vector_index,
                       type_name(tok->datatype), tok->vector_length);
            else
                printf("assigning values to %s{%i}[%i] (%s x %d)\n",
                       expr->variables[tok->var].name, tok->history_index,
                       tok->vector_index, type_name(tok->datatype),
                       tok->vector_length);
#endif
            updated++;
            if (tok->var == VAR_Y) {
                int idx = (tok->history_index + output->position + output->size);
                if (idx < 0)
                    idx = output->size - idx;
                else
                    idx %= output->size;

                switch (output->type) {
                case 'f': {
                    float *v = (output->value + idx * output->length
                                * mapper_type_size(output->type));
                    for (i = 0; i < tok->vector_length; i++)
                        v[i + tok->vector_index] = stack[top][i + tok->assignment_offset].f;
                    break;
                }
                case 'i': {
                    int *v = (output->value + idx * output->length
                              * mapper_type_size(output->type));
                    for (i = 0; i < tok->vector_length; i++)
                        v[i + tok->vector_index] = stack[top][i + tok->assignment_offset].i32;
                    break;
                }
                case 'd': {
                    double *v = (output->value + idx * output->length
                                 * mapper_type_size(output->type));
                    for (i = 0; i < tok->vector_length; i++)
                        v[i + tok->vector_index] = stack[top][i + tok->assignment_offset].d;
                    break;
                }
                default:
                    goto error;
                }

                if (typestring) {
                    for (i = tok->vector_index; i < tok->vector_index + tok->vector_length; i++) {
                        typestring[i] = tok->datatype;
                    }
                }
            }
            else if (tok->var >= 0 && tok->var < N_USER_VARS) {
                if (!expr_vars)
                    goto error;
                // passed the address of an array of mapper_signal_history structs
                mapper_history h = *expr_vars + tok->var;

                // increment position
                h->position = (h->position + 1) % h->size;

                mapper_variable var = &expr->variables[tok->var];
                int idx = (tok->history_index + h->position
                           + var->history_size) % var->history_size;
                double *v = h->value + idx * var->vector_length * mapper_type_size(var->datatype);
                for (i = 0; i < tok->vector_length; i++)
                    v[i + tok->vector_index] = stack[top][i + tok->assignment_offset].d;

                // Also copy timetag from input
                if (tt) {
                    mapper_timetag_t *ttvar = mapper_history_tt_ptr(*h);
                    memcpy(ttvar, tt, sizeof(mapper_timetag_t));
                }

                expr->variables[tok->var].assigned = 1;
            }
            else
                goto error;

            /* If assignment was history initialization, move expression start
             * token pointer so we don't evaluate this section again. */
            if (tok->history_index != 0) {
                expr->start_offset = tok - expr->start + 1;
            }
            break;
        default: goto error;
        }
        if (tok->casttype && tok->toktype < TOK_ASSIGNMENT) {
#if TRACING
            printf("casting from %s to %s\n", type_name(tok->datatype),
                   type_name(tok->casttype));
#endif
            // need to cast to a different type
            switch (tok->datatype) {
            case 'f':
                switch (tok->casttype) {
                case 'i':
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].i32 = (int)stack[top][i].f;
                    }
                    break;
                case 'd':
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].d = (double)stack[top][i].f;
                    }
                    break;
                default:
                    goto error;
                }
                break;
            case 'i':
                switch (tok->casttype) {
                case 'f':
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].f = (float)stack[top][i].i32;
                    }
                    break;
                case 'd':
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].d = (double)stack[top][i].i32;
                    }
                    break;
                default:
                    goto error;
                }
                break;
            case 'd':
                switch (tok->casttype) {
                case 'f':
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].f = (float)stack[top][i].d;
                    }
                    break;
                case 'i':
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].i32 = (int)stack[top][i].d;
                    }
                    break;
                default:
                    goto error;
                }
                break;
            default:
                goto error;
            }
        }
        tok++;
        count++;
    }

    if (!typestring) {
        /* Internal evaluation during parsing doesn't contain assignment token,
         * so we need to copy to output here. */

        /* Increment index position of output data structure. */
        output->position = (output->position + 1) % output->size;

        switch (output->type) {
        case 'f': {
            float *v = mapper_history_value_ptr(*output);
            for (i = 0; i < output->length; i++)
                v[i] = stack[top][i].f;
            break;
        }
        case 'i': {
            int *v = mapper_history_value_ptr(*output);
            for (i = 0; i < output->length; i++)
                v[i] = stack[top][i].i32;
            break;
        }
        case 'd': {
            double *v = mapper_history_value_ptr(*output);
            for (i = 0; i < output->length; i++)
                v[i] = stack[top][i].d;
            break;
        }
        default:
            goto error;
        }
        return 1;
    }

    /* Undo position increment if nothing was updated. */
    if (!updated) {
        --output->position;
        if (output->position < 0)
            output->position = output->size - 1;
        return 0;
    }
    else if (tt) {
        // Also copy timetag from input
        mapper_timetag_t *ttto = mapper_history_tt_ptr(*output);
        memcpy(ttto, tt, sizeof(mapper_timetag_t));
    }

    for (i = 0; i < expr->num_variables; i++) {
        if (expr->variables[i].assigned) {
#if TRACING
            printf("incrementing position for variable %s\n",
                   expr->variables[i].name);
#endif
            // increment position
            mapper_history h = *expr_vars + i;
            h->position = (h->position + 1) % h->size;
        }
    }

    return 1;

  error:
    trace("Unexpected token in expression.");
    return 0;
}
