/********************************************************************
** Copyright (c) 2018-2020 Guan Wenliang
** This file is part of the Berry default interpreter.
** skiars@qq.com, https://github.com/Skiars/berry
** See Copyright Notice in the LICENSE file or at
** https://github.com/Skiars/berry/blob/master/LICENSE
********************************************************************/
#include "be_object.h"
#include "be_mem.h"
#include "be_lexer.h"
#include <string.h>
#include <math.h>

#if BE_USE_JSON_MODULE

#define is_space(c)     ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#define is_digit(c)     ((c) >= '0' && (c) <= '9')

#define MAX_INDENT      24
#define INDENT_WIDTH    2
#define INDENT_CHAR     ' '

static const char* parser_value(bvm *vm, const char *json);
static void value_dump(bvm *vm, int *indent, int idx, int fmt);

static const char* skip_space(const char *s)
{
    int c;
    while (((c = *s) != '\0') && ((c == ' ')
        || (c == '\t') || (c == '\r') || (c == '\n'))) {
        ++s;
    }
    return s;
}

static const char* match_char(const char *json, int ch)
{
    json = skip_space(json);
    if (*json == ch) {
        return skip_space(json + 1);
    }
    return NULL;
}

static int is_object(bvm *vm, const char *class, int idx)
{
    if (be_isinstance(vm, idx)) {
        be_pushvalue(vm, idx);
        while (1) {
            be_getsuper(vm, -1);
            if (be_isnil(vm, -1)) {
                be_pop(vm, 1);
                break;
            }
            be_remove(vm, -2);
        }
        const char *name = be_classname(vm, -1);
        bbool ret = !strcmp(name, class);
        be_pop(vm, 1);
        return ret;
    }
    return  0;
}

static int json_strlen(const char *json)
{
    int ch;
    const char *s = json + 1; /* skip '"' */
    /* get string length "(\\.|[^"])*" */
    while ((ch = *s) != '\0' && ch != '"') {
        ++s;
        if (ch == '\\') {
            ch = *s++;
            if (ch == '\0') {
                return -1;
            }
        }
    }
    return ch ? cast_int(s - json - 1) : -1;
}

static void json2berry(bvm *vm, const char *class)
{
    be_getbuiltin(vm, class);
    be_pushvalue(vm, -2);
    be_call(vm, 1);
    be_moveto(vm, -2, -3);
    be_pop(vm, 2);
}

static const char* parser_true(bvm *vm, const char *json)
{
    if (!strncmp(json, "true", 4)) {
        be_pushbool(vm, btrue);
        return json + 4;
    }
    return NULL;
}

static const char* parser_false(bvm *vm, const char *json)
{
    if (!strncmp(json, "false", 5)) {
        be_pushbool(vm, bfalse);
        return json + 5;
    }
    return NULL;
}

static const char* parser_null(bvm *vm, const char *json)
{
    if (!strncmp(json, "null", 4)) {
        be_pushnil(vm);
        return json + 4;
    }
    return NULL;
}

static const char* parser_string(bvm *vm, const char *json)
{
    if (*json == '"') {
        int len = json_strlen(json++);
        if (len > -1) {
            int ch;
            char *buf, *dst = buf = be_malloc(vm, len);
            while ((ch = *json) != '\0' && ch != '"') {
                ++json;
                if (ch == '\\') {
                    ch = *json++; /* skip '\' */
                    switch (ch) {
                    case '"': *dst++ = '"'; break;
                    case '\\': *dst++ = '\\'; break;
                    case '/': *dst++ = '/'; break;
                    case 'b': *dst++ = '\b'; break;
                    case 'f': *dst++ = '\f'; break;
                    case 'n': *dst++ = '\n'; break;
                    case 'r': *dst++ = '\r'; break;
                    case 't': *dst++ = '\t'; break;
                    case 'u': { /* load unicode */
                        dst = be_load_unicode(dst, json);
                        if (dst == NULL) {
                            be_free(vm, buf, len);
                            return NULL;
                        }
                        json += 4;
                        break;
                    }
                    default: be_free(vm, buf, len); return NULL; /* error */
                    }
                } else if(ch >= 0 && ch <= 0x1f) {
                    /* control characters must be escaped
                       as per https://www.rfc-editor.org/rfc/rfc7159#section-7 */
                    be_free(vm, buf, len);
                    return NULL;
                } else {
                    *dst++ = (char)ch;
                }
            }
            be_assert(ch == '"');
            /* require the stack to have some free space for the string, 
               since parsing deeply nested objects might
               crash the VM due to insufficient stack space. */
            be_stack_require(vm, 1 + BE_STACK_FREE_MIN);
            be_pushnstring(vm, buf, cast_int(dst - buf));
            be_free(vm, buf, len);
            return json + 1; /* skip '"' */
        }
    }
    return NULL;
}

static const char* parser_field(bvm *vm, const char *json)
{
    be_stack_require(vm, 2 + BE_STACK_FREE_MIN);
    if (json && *json == '"') {
        json = parser_string(vm, json);
        if (json) {
            json = match_char(json, ':');
            if (json) {
                json = parser_value(vm, json);
                if (json) {
                    be_data_insert(vm, -3);
                    be_pop(vm, 2); /* pop key and value */
                    return json;
                }
            }
            be_pop(vm, 1); /* pop key */
        }
    }
    return NULL;
}

static const char* parser_object(bvm *vm, const char *json)
{
    json = match_char(json, '{');
    be_newmap(vm);
    if (*json != '}') {
        const char *s;
        json = parser_field(vm, json);
        if (json == NULL) {
            be_pop(vm, 1); /* pop map */
            return NULL;
        }
        while ((s = match_char(json, ',')) != NULL) {
            json = parser_field(vm, s);
            if (json == NULL) {
                be_pop(vm, 1); /* pop map */
                return NULL;
            }
        }
    }
    if ((json = match_char(json, '}')) == NULL) {
        be_pop(vm, 1); /* pop map */
        return NULL;
    }
    json2berry(vm, "map");
    return json;
}

static const char* parser_array(bvm *vm, const char *json)
{
    json = match_char(json, '[');
    be_newlist(vm);
    if (*json != ']') {
        const char *s;
        json = parser_value(vm, json);
        if (json == NULL) {
            be_pop(vm, 1); /* pop map */
            return NULL;
        }
        be_data_push(vm, -2);
        be_pop(vm, 1); /* pop value */
        while ((s = match_char(json, ',')) != NULL) {
            json = parser_value(vm, s);
            if (json == NULL) {
                be_pop(vm, 1); /* pop map */
                return NULL;
            }
            be_data_push(vm, -2);
            be_pop(vm, 1); /* pop value */
        }
    }
    if ((json = match_char(json, ']')) == NULL) {
        be_pop(vm, 1); /* pop map */
        return NULL;
    }
    json2berry(vm, "list");
    return json;
}

enum {
    JSON_NUMBER_INVALID = 0,
    JSON_NUMBER_INTEGER = 1,
    JSON_NUMBER_REAL = 2
};

int check_json_number(const char *json) {
    if (!json || *json == '\0') {
        return JSON_NUMBER_INVALID;
    }
    
    const char *p = json;
    bbool has_fraction = bfalse;
    bbool has_exponent = bfalse;
    
    // Skip leading whitespace
    while (is_space(*p)) {
        p++;
    }
    
    if (*p == '\0') {
        return JSON_NUMBER_INVALID;
    }
    
    // Handle optional minus sign
    if (*p == '-') {
        p++;
        if (*p == '\0') {
            return JSON_NUMBER_INVALID;
        }
    }
    
    // Integer part
    if (*p == '0') {
        // If starts with 0, next char must not be a digit (unless it's decimal point or exponent)
        p++;
        if (is_digit(*p)) {
            return JSON_NUMBER_INVALID; // Leading zeros not allowed (except standalone 0)
        }
    } else if (is_digit(*p)) {
        // First digit must be 1-9, then any digits
        p++;
        while (is_digit(*p)) {
            p++;
        }
    } else {
        return JSON_NUMBER_INVALID; // Must start with digit
    }
    
    // Optional fractional part
    if (*p == '.') {
        has_fraction = btrue;
        p++;
        if (!is_digit(*p)) {
            return JSON_NUMBER_INVALID; // Must have at least one digit after decimal point
        }
        while (is_digit(*p)) {
            p++;
        }
    }
    
    // Optional exponent part
    if (*p == 'e' || *p == 'E') {
        has_exponent = btrue;
        p++;
        // Optional sign in exponent
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!is_digit(*p)) {
            return JSON_NUMBER_INVALID; // Must have at least one digit in exponent
        }
        while (is_digit(*p)) {
            p++;
        }
    }
    
    // Number ends here - check that next char is not a continuation
    // Valid JSON number termination: whitespace, null, or JSON delimiters
    if (*p != '\0' && !is_space(*p) && *p != ',' && *p != ']' && *p != '}' && *p != ':') {
        return JSON_NUMBER_INVALID;
    }
    
    // Determine return value based on what was found
    // Any number with exponent (e/E) is always real, regardless of fractional part
    if (has_exponent || has_fraction) {
        return JSON_NUMBER_REAL; // real number
    } else {
        return JSON_NUMBER_INTEGER; // integer
    }
}

static const char* parser_number(bvm *vm, const char *json)
{
    const char *endstr = NULL;
    int number_type = check_json_number(json);
    
    switch (number_type) {
    case JSON_NUMBER_INTEGER:
        be_pushint(vm, be_str2int(json, &endstr));
        break;
    case JSON_NUMBER_REAL:
        be_pushreal(vm, be_str2real(json, &endstr));
        break;
    default:
        endstr = NULL;
    }
    return endstr;
}

/* parser json value */
static const char* parser_value(bvm *vm, const char *json)
{
    json = skip_space(json);
    /*
      Each value will push at least one thig to the stack, so we must ensure it's big enough.
      We need to take special care to extend the stack in values which have variable length (arrays and objects)
    */
    be_stack_require(vm, 1 + BE_STACK_FREE_MIN);
    switch (*json) {
    case '{': /* object */
        return parser_object(vm, json);
    case '[': /* array */
        return parser_array(vm, json);
    case '"': /* string */
        return parser_string(vm, json);
    case 't': /* true */
        return parser_true(vm, json);
    case 'f': /* false */
        return parser_false(vm, json);
    case 'n': /* null */
        return parser_null(vm, json);
    default: /* number */
        if (*json == '-' || is_digit(*json)) {
           return parser_number(vm, json);
        }
    }
    return NULL;
}

static int m_json_load(bvm *vm)
{
    if (be_isstring(vm, 1)) {
        const char *json = be_tostring(vm, 1);
        json = parser_value(vm, json);
        if (json != NULL && *json == '\0') {
            be_return(vm);
        }
    }
    be_return_nil(vm);
}

static void make_indent(bvm *vm, int stridx, int indent)
{
    if (indent) {
        be_stack_require(vm, 1 + BE_STACK_FREE_MIN); 
        char buf[MAX_INDENT * INDENT_WIDTH + 1];
        indent = (indent < MAX_INDENT ? indent : MAX_INDENT) * INDENT_WIDTH;
        memset(buf, INDENT_CHAR, indent);
        buf[indent] = '\0';
        stridx = be_absindex(vm, stridx);
        be_pushstring(vm, buf);
        be_strconcat(vm, stridx);
        be_pop(vm, 1);
    }
}

void string_dump(bvm *vm, int index)
{
    be_stack_require(vm, 1 + BE_STACK_FREE_MIN); 
    be_tostring(vm, index); /* convert value to string */
    be_toescape(vm, index, 'u');
    be_pushvalue(vm, index);
}

static void object_dump(bvm *vm, int *indent, int idx, int fmt)
{

    be_stack_require(vm, 3 + BE_STACK_FREE_MIN); /* 3 pushes outside the loop */
    be_getmember(vm, idx, ".p");
    be_pushstring(vm, fmt ? "{\n" : "{");
    be_pushiter(vm, -2); /* map iterator use 1 register */
    *indent += fmt;
    while (be_iter_hasnext(vm, -3)) {
        be_stack_require(vm, 3 + BE_STACK_FREE_MIN); /* 3 pushes inside the loop */
        make_indent(vm, -2, fmt ? *indent : 0);
        be_iter_next(vm, -3);
        /* key.tostring() */
        string_dump(vm, -2);
        be_strconcat(vm, -5);
        be_pop(vm, 1);
        be_pushstring(vm, fmt ? ": " : ":"); /* add ': ' */
        be_strconcat(vm, -5);
        be_pop(vm, 1);
        /* value.tostring() */
        value_dump(vm, indent, -1, fmt);
        be_strconcat(vm, -5);
        be_pop(vm, 3);
        if (be_iter_hasnext(vm, -3)) {
            be_pushstring(vm, fmt ? ",\n" : ",");
            be_strconcat(vm, -3);
            be_pop(vm, 1);
        } else if (fmt) {
            be_pushstring(vm, "\n");
            be_strconcat(vm, -3);
            be_pop(vm, 1);
        }
    }
    *indent -= fmt;
    be_pop(vm, 1); /* pop iterator */
    make_indent(vm, -1,  fmt ? *indent : 0);
    be_pushstring(vm, "}");
    be_strconcat(vm, -2);
    be_moveto(vm, -2, -3);
    be_pop(vm, 2);
}

static void array_dump(bvm *vm, int *indent, int idx, int fmt)
{
    be_stack_require(vm, 3 + BE_STACK_FREE_MIN); 
    be_getmember(vm, idx, ".p");
    be_pushstring(vm, fmt ? "[\n" : "[");
    be_pushiter(vm, -2);
    *indent += fmt;
    while (be_iter_hasnext(vm, -3)) {
        make_indent(vm, -2,  fmt ? *indent : 0);
        be_iter_next(vm, -3);
        value_dump(vm, indent, -1, fmt);
        be_strconcat(vm, -4);
        be_pop(vm, 2);
        be_stack_require(vm, 1 + BE_STACK_FREE_MIN); 
        if (be_iter_hasnext(vm, -3)) {
            be_pushstring(vm, fmt ? ",\n" : ",");
            be_strconcat(vm, -3);
            be_pop(vm, 1);
        } else if (fmt) {
            be_pushstring(vm, "\n");
            be_strconcat(vm, -3);
            be_pop(vm, 1);
        }
    }
    *indent -= fmt;
    be_pop(vm, 1); /* pop iterator */
    make_indent(vm, -1,  fmt ? *indent : 0);
    be_pushstring(vm, "]");
    be_strconcat(vm, -2);
    be_moveto(vm, -2, -3);
    be_pop(vm, 2);
}

static void value_dump(bvm *vm, int *indent, int idx, int fmt)
{
    // be_stack_require(vm, 1 + BE_STACK_FREE_MIN);
    if (is_object(vm, "map", idx)) { /* convert to json object */
        object_dump(vm, indent, idx, fmt);
    } else if (is_object(vm, "list", idx)) { /* convert to json array */
        array_dump(vm, indent, idx, fmt);
    } else if (be_isnil(vm, idx)) { /* convert to json null */
        be_stack_require(vm, 1 + BE_STACK_FREE_MIN); 
        be_pushstring(vm, "null");
    } else if (be_isreal(vm, idx)) {
        be_stack_require(vm, 1 + BE_STACK_FREE_MIN);
        breal v = be_toreal(vm, idx);
        if (isnan(v) || isinf(v)) {
            be_pushstring(vm, "null");
        } else {
            be_tostring(vm, idx);
            be_pushvalue(vm, idx); /* push to top */
        };
    } else if (be_isnumber(vm, idx) || be_isbool(vm, idx)) { /* convert to json number and boolean */
        be_stack_require(vm, 1 + BE_STACK_FREE_MIN); 
        be_tostring(vm, idx);
        be_pushvalue(vm, idx); /* push to top */
    } else { /* convert to string */
        string_dump(vm, idx);
    }
}

static int m_json_dump(bvm *vm)
{
    int indent = 0, argc = be_top(vm);
    int fmt = 0;
    if (argc > 1) {
        fmt = !strcmp(be_tostring(vm, 2), "format");
    }
    value_dump(vm, &indent, 1, fmt);
    be_return(vm);
}

#if !BE_USE_PRECOMPILED_OBJECT
be_native_module_attr_table(json) {
    be_native_module_function("load", m_json_load),
    be_native_module_function("dump", m_json_dump)
};

be_define_native_module(json, NULL);
#else
/* @const_object_info_begin
module json (scope: global, depend: BE_USE_JSON_MODULE) {
    load, func(m_json_load)
    dump, func(m_json_dump)
}
@const_object_info_end */
#include "../generate/be_fixed_json.h"
#endif

#endif /* BE_USE_JSON_MODULE */
