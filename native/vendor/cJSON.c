/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  cJSON - Lightweight JSON parser in ANSI C
*/

#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <limits.h>

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

void cJSON_InitHooks(cJSON_Hooks* hooks) {
    if (!hooks) {
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }
    cJSON_malloc = (hooks->malloc_fn) ? hooks->malloc_fn : malloc;
    cJSON_free   = (hooks->free_fn) ? hooks->free_fn : free;
}

static cJSON *cJSON_New_Item(void) {
    cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node) memset(node, '\0', sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *c) {
    cJSON *next;
    while (c) {
        next = c->next;
        if (!(c->type & cJSON_IsReference) && c->child) cJSON_Delete(c->child);
        if (!(c->type & cJSON_IsReference) && c->valuestring) cJSON_free(c->valuestring);
        if (!(c->type & cJSON_StringIsConst) && c->string) cJSON_free(c->string);
        cJSON_free(c);
        c = next;
    }
}

static const char *skip(const char *in) {
    while (in && *in && ((unsigned char)*in <= 32)) in++;
    return in;
}

static const char *parse_value(cJSON *item, const char *value);
static const char *parse_string(cJSON *item, const char *str);
static const char *parse_number(cJSON *item, const char *num);
static const char *parse_array(cJSON *item, const char *value);
static const char *parse_object(cJSON *item, const char *value);

static const char *parse_number(cJSON *item, const char *num) {
    double n = 0;
    char *endptr = NULL;
    n = strtod(num, &endptr);
    if (endptr == num) return NULL;
    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return endptr;
}

static const char *parse_string(cJSON *item, const char *str) {
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;
    if (*str != '\"') return NULL;

    while (*ptr != '\"' && *ptr && ++len) {
        if (*ptr++ == '\\') ptr++;
    }

    out = (char*)cJSON_malloc(len + 1);
    if (!out) return NULL;

    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') {
            *ptr2++ = *ptr++;
        } else {
            ptr++;
            switch (*ptr) {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                case '\"': case '\\': case '/': *ptr2++ = *ptr; break;
                default: *ptr2++ = *ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = 0;
    if (*ptr == '\"') ptr++;
    item->valuestring = out;
    item->type = cJSON_String;
    return ptr;
}

static const char *parse_array(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '[') return NULL;

    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_value(child, skip(value)));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return NULL;
    }

    if (*value == ']') return value + 1;
    return NULL;
}

static const char *parse_object(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '{') return NULL;

    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;
    value = skip(parse_string(child, skip(value)));
    if (!value) return NULL;
    child->string = child->valuestring;
    child->valuestring = NULL;
    if (*value != ':') return NULL;
    value = skip(parse_value(child, skip(value + 1)));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value) return NULL;
        child->string = child->valuestring;
        child->valuestring = NULL;
        if (*value != ':') return NULL;
        value = skip(parse_value(child, skip(value + 1)));
        if (!value) return NULL;
    }

    if (*value == '}') return value + 1;
    return NULL;
}

static const char *parse_value(cJSON *item, const char *value) {
    if (!value) return NULL;
    if (!strncmp(value, "null", 4))  { item->type = cJSON_NULL; return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4))  { item->type = cJSON_True; item->valueint = 1; return value + 4; }
    if (*value == '\"')              { return parse_string(item, value); }
    if (*value == '-' || (*value >= '0' && *value <= '9')) { return parse_number(item, value); }
    if (*value == '[')              { return parse_array(item, value); }
    if (*value == '{')              { return parse_object(item, value); }
    return NULL;
}

cJSON *cJSON_Parse(const char *value) {
    return cJSON_ParseWithLength(value, value ? strlen(value) : 0);
}

cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length) {
    (void)buffer_length;
    cJSON *c = cJSON_New_Item();
    if (!c) return NULL;
    const char *end = parse_value(c, skip(value));
    if (!end) { cJSON_Delete(c); return NULL; }
    return c;
}

cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string) {
    cJSON *c = object ? object->child : NULL;
    while (c && c->string && _stricmp(c->string, string)) c = c->next;
    return c;
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string) {
    cJSON *c = object ? object->child : NULL;
    while (c && c->string && strcmp(c->string, string)) c = c->next;
    return c;
}

int cJSON_HasObjectItem(const cJSON *object, const char *string) {
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

int cJSON_GetArraySize(const cJSON *array) {
    cJSON *c = array ? array->child : NULL;
    int i = 0;
    while (c) i++, c = c->next;
    return i;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    cJSON *c = array ? array->child : NULL;
    while (c && index > 0) index--, c = c->next;
    return c;
}

cJSON *cJSON_CreateNull(void)   { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_NULL; return item; }
cJSON *cJSON_CreateTrue(void)   { cJSON *item = cJSON_New_Item(); if (item) { item->type = cJSON_True; item->valueint = 1; } return item; }
cJSON *cJSON_CreateFalse(void)  { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_False; return item; }
cJSON *cJSON_CreateBool(int b)  { return b ? cJSON_CreateTrue() : cJSON_CreateFalse(); }
cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (item) { item->type = cJSON_Number; item->valuedouble = num; item->valueint = (int)num; }
    return item;
}
cJSON *cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_String;
        item->valuestring = _strdup(string);
    }
    return item;
}
cJSON *cJSON_CreateArray(void)  { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Array; return item; }
cJSON *cJSON_CreateObject(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Object; return item; }

void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    if (!array || !item) return;
    if (!array->child) {
        array->child = item;
    } else {
        cJSON *c = array->child;
        while (c->next) c = c->next;
        c->next = item;
        item->prev = c;
    }
}

void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!object || !string || !item) return;
    if (item->string) cJSON_free(item->string);
    item->string = _strdup(string);
    cJSON_AddItemToArray(object, item);
}

