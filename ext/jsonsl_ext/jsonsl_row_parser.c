/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Author:: Couchbase <info@couchbase.com>
 * Copyright:: 2018 Couchbase, Inc.
 * License:: Apache License, Version 2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jsonsl_ext.h"

VALUE jsl_cRowParser;

ID jsl_id_call;
ID jsl_sym_root;
ID jsl_sym_rows;

typedef struct jsl_PARSER {
    jsonsl_t jsn;
    jsonsl_jpr_t ptr;
    VALUE buffer;
    VALUE proc;
    VALUE last_key;
    int initialized;
    size_t last_row_endpos;
    size_t header_len;
    int rowcount;
} jsl_PARSER;

static void jsl_parser_mark(void *ptr)
{
    jsl_PARSER *parser = ptr;
    if (parser) {
        rb_gc_mark_maybe(parser->buffer);
        rb_gc_mark_maybe(parser->proc);
        rb_gc_mark_maybe(parser->last_key);
    }
}

static void jsl_parser_free(void *ptr)
{
    jsl_PARSER *parser = ptr;
    if (parser) {
        if (parser->jsn) {
            jsonsl_destroy(parser->jsn);
        }
        parser->jsn = NULL;
        if (parser->ptr) {
            jsonsl_jpr_destroy(parser->ptr);
        }
        parser->ptr = NULL;
        ruby_xfree(parser);
    }
}

static VALUE jsl_parser_alloc(VALUE klass)
{
    VALUE obj;
    jsl_PARSER *parser;

    obj = Data_Make_Struct(klass, jsl_PARSER, jsl_parser_mark, jsl_parser_free, parser);
    return obj;
}

static int jsl_parser_error_callback(jsonsl_t jsn, jsonsl_error_t err, struct jsonsl_state_st *state, char *at)
{
    char buf[30] = {0};
    sprintf(buf, "error at %d position", (int)jsn->pos);
    jsl_raise(err, buf);
    (void)at;
    (void)state;
    return 0;
}

static void jsl_parser_cover_push_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                           const jsonsl_char_t *at)
{
    jsl_PARSER *parser = (jsl_PARSER *)jsn->data;
    parser->header_len = state->pos_begin;
    jsn->action_callback_PUSH = NULL;
    (void)action;
    (void)at;
}

static void jsl_parser_reset(jsl_PARSER *parser)
{
    if (parser) {
        parser->buffer = Qnil;
        parser->last_key = Qnil;
    }
}

static void jsl_parser_cover_pop_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                          const jsonsl_char_t *at)
{
    jsl_PARSER *parser = (jsl_PARSER *)jsn->data;
    VALUE cover;

    if (state->val != jsl_sym_root) {
        return;
    }
    cover = rb_str_new(RSTRING_PTR(parser->buffer), parser->header_len);
    rb_str_cat(cover, RSTRING_PTR(parser->buffer) + parser->last_row_endpos,
               RSTRING_LEN(parser->buffer) - parser->last_row_endpos);
    rb_funcall(parser->proc, jsl_id_call, 1, cover);
    jsl_parser_reset(parser);
    (void)action;
    (void)at;
}

static void jsl_parser_row_pop_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                        const jsonsl_char_t *at)
{
    jsl_PARSER *parser = (jsl_PARSER *)jsn->data;
    parser->last_row_endpos = jsn->pos;

    if (state->val == jsl_sym_rows) {
        jsn->action_callback_POP = jsl_parser_cover_pop_callback;
        jsn->action_callback_PUSH = NULL;
        if (parser->rowcount == 0) {
            parser->header_len = state->pos_begin + 1;
        }
        return;
    }

    const char *ptr = RSTRING_PTR(parser->buffer) + state->pos_begin;
    size_t len = jsn->pos - state->pos_begin + 1;
    if (state->type == JSONSL_T_SPECIAL) {
        len--;
    }
    rb_funcall(parser->proc, jsl_id_call, 2, INT2FIX(parser->rowcount), rb_str_new(ptr, len));
    parser->rowcount++;

    (void)action;
    (void)at;
}

static void jsl_parser_initial_push_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                             const jsonsl_char_t *at)
{
    jsl_PARSER *parser = (jsl_PARSER *)jsn->data;
    jsonsl_jpr_match_t match = JSONSL_MATCH_UNKNOWN;
    if (JSONSL_STATE_IS_CONTAINER(state)) {
        jsonsl_jpr_match_state(jsn, state, RSTRING_PTR(parser->last_key), RSTRING_LEN(parser->last_key), &match);
    }
    if (parser->initialized == 0) {
        if (state->type != JSONSL_T_OBJECT) {
            jsl_raise_msg("expected root to be an object");
        }
        if (match != JSONSL_MATCH_POSSIBLE) {
            jsl_raise_msg("root does not match JSON pointer");
        }
        state->val = jsl_sym_root;
        parser->initialized = 1;
    }

    if (state->type == JSONSL_T_LIST && match == JSONSL_MATCH_POSSIBLE) {
        state->val = jsl_sym_rows;
        jsn->action_callback_POP = jsl_parser_row_pop_callback;
        jsn->action_callback_PUSH = jsl_parser_cover_push_callback;
    }
    (void)action;
    (void)at;
}

static void jsl_parser_initial_pop_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                            const jsonsl_char_t *at)
{
    jsl_PARSER *parser = (jsl_PARSER *)jsn->data;
    if (state->type == JSONSL_T_HKEY) {
        const char *key_str = RSTRING_PTR(parser->buffer) + state->pos_begin + 1;
        size_t len = jsn->pos - state->pos_begin - 1;
        parser->last_key = rb_str_new(key_str, len);
    }
    (void)action;
    (void)at;
}

static VALUE jsl_parser_init(int argc, VALUE *argv, VALUE self)
{
    jsl_PARSER *parser = DATA_PTR(self);
    VALUE nlevels = Qnil;
    VALUE jptr = Qnil;
    VALUE proc = Qnil;
    jsonsl_error_t rc = JSONSL_ERROR_SUCCESS;

    rb_scan_args(argc, argv, "11&", &jptr, &nlevels, &proc);
    if (proc == Qnil) {
        rb_raise(rb_eArgError, "tried to create Parser object without a block");
    }

    Check_Type(jptr, T_STRING);
    parser->ptr = jsonsl_jpr_new(RSTRING_PTR(jptr), &rc);
    if (rc != JSONSL_ERROR_SUCCESS) {
        jsl_raise(rc, "invalid JSON pointer");
    }
    if (nlevels != Qnil) {
        Check_Type(nlevels, T_FIXNUM);
        parser->jsn = jsonsl_new(FIX2INT(nlevels));
    } else {
        parser->jsn = jsonsl_new(JSONSL_MAX_LEVELS);
    }
    parser->initialized = 0;
    parser->buffer = rb_str_buf_new(100);
    parser->last_key = rb_str_new_cstr("");
    parser->proc = proc;
    parser->jsn->data = parser;
    parser->jsn->max_callback_level = 4;
    jsonsl_jpr_match_state_init(parser->jsn, &parser->ptr, 1);
    jsonsl_reset(parser->jsn);
    parser->jsn->error_callback = jsl_parser_error_callback;
    parser->jsn->action_callback_PUSH = jsl_parser_initial_push_callback;
    parser->jsn->action_callback_POP = jsl_parser_initial_pop_callback;
    jsonsl_enable_all_callbacks(parser->jsn);
    return self;
}

static VALUE jsl_parser_inspect(VALUE self)
{
    jsl_PARSER *parser = DATA_PTR(self);
    VALUE str;

    str = rb_str_buf_new2("#<");
    rb_str_buf_cat2(str, rb_obj_classname(self));
    rb_str_catf(str, ":%p", (void *)self);
    if (parser->buffer != Qnil) {
        rb_str_catf(str, " buflen=%lu", (long int)RSTRING_LEN(parser->buffer));
    }
    if (parser->ptr && parser->ptr->orig) {
        VALUE tmp = rb_inspect(rb_str_new_cstr(parser->ptr->orig));
        rb_str_catf(str, " ptr=%s", RSTRING_PTR(tmp));
    }
    rb_str_buf_cat_ascii(str, ">");

    return str;
}

static VALUE jsl_parser_feed(VALUE self, VALUE data)
{
    jsl_PARSER *parser = DATA_PTR(self);
    size_t old_len;

    if (NIL_P(parser->buffer)) {
        return self;
    }
    Check_Type(data, T_STRING);
    old_len = RSTRING_LEN(parser->buffer);
    rb_str_buf_append(parser->buffer, data);
    jsonsl_feed(parser->jsn, RSTRING_PTR(parser->buffer) + old_len, RSTRING_LEN(data));

    return self;
}

void jsl_row_parser_init()
{
    jsl_id_call = rb_intern("call");
    jsl_sym_root = ID2SYM(rb_intern("root"));
    jsl_sym_rows = ID2SYM(rb_intern("row"));

    jsl_cRowParser = rb_define_class_under(jsl_mJSONSL, "RowParser", rb_cObject);
    rb_define_alloc_func(jsl_cRowParser, jsl_parser_alloc);
    rb_define_method(jsl_cRowParser, "initialize", jsl_parser_init, -1);
    rb_define_method(jsl_cRowParser, "inspect", jsl_parser_inspect, 0);
    rb_define_method(jsl_cRowParser, "feed", jsl_parser_feed, 1);
}
