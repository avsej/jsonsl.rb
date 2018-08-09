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

VALUE jsl_mJSONSL;
VALUE jsl_eError;

void jsl_raise_at(jsonsl_error_t code, const char *message, const char *file, int line)
{
    VALUE exc, str;

    str = rb_str_new_cstr(message);
    if (code > 0) {
        rb_str_catf(str, ": (0x%02x) \"%s\"", (int)code, jsonsl_strerror(code));
    }
    rb_str_buf_cat_ascii(str, " [");
    while (*file == '.' || *file == '/') {
        file++;
    }
    rb_str_buf_cat_ascii(str, file);
    rb_str_catf(str, ":%d]", line);
    exc = rb_exc_new3(jsl_eError, str);
    rb_ivar_set(exc, rb_intern("@code"), INT2FIX(code));
    rb_exc_raise(exc);
}

static int jsl_jsonsl_error_callback(jsonsl_t jsn, jsonsl_error_t err, struct jsonsl_state_st *state, char *at)
{
    char buf[30] = {0};
    sprintf(buf, "error at %d position", (int)jsn->pos);
    jsl_raise(err, buf);
    (void)at;
    (void)state;
    return 0;
}

static void jsl_jsonsl_push_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                     const jsonsl_char_t *at)
{
    switch (state->type) {
        case JSONSL_T_SPECIAL:
        case JSONSL_T_STRING:
            break;
        case JSONSL_T_HKEY:
            break;
        case JSONSL_T_LIST:
            state->val = rb_ary_new();
            break;
        case JSONSL_T_OBJECT:
            state->val = rb_hash_new();
            break;
        default:
            jsl_raise_msg("unexpected state type in POP callback");
            break;
    }
    (void)at;
    (void)jsn;
    (void)action;
}

static void jsl_jsonsl_pop_callback(jsonsl_t jsn, jsonsl_action_t action, struct jsonsl_state_st *state,
                                    const jsonsl_char_t *at)
{
    struct jsonsl_state_st *last_state = jsonsl_last_state(jsn, state);
    VALUE val = Qnil;

    switch (state->type) {
        case JSONSL_T_SPECIAL:
            if (state->special_flags & JSONSL_SPECIALf_NUMNOINT) {
                val = rb_float_new(strtod((char *)jsn->base + state->pos_begin, NULL));
            } else if (state->special_flags & JSONSL_SPECIALf_NUMERIC) {
                val = rb_cstr2inum((char *)jsn->base + state->pos_begin, 10);
            } else if (state->special_flags & JSONSL_SPECIALf_TRUE) {
                val = Qtrue;
            } else if (state->special_flags & JSONSL_SPECIALf_FALSE) {
                val = Qfalse;
            } else if (state->special_flags & JSONSL_SPECIALf_NULL) {
                val = Qnil;
            } else {
                jsl_raise_msg("invalid special value");
            }
            break;
        case JSONSL_T_STRING:
            val = rb_str_new((char *)jsn->base + state->pos_begin + 1, at - ((char *)jsn->base + state->pos_begin + 1));
            break;
        case JSONSL_T_HKEY:
            val = rb_str_new((char *)jsn->base + state->pos_begin + 1, at - ((char *)jsn->base + state->pos_begin + 1));
            break;
        case JSONSL_T_LIST:
        case JSONSL_T_OBJECT:
            val = (VALUE)state->val;
            break;
        default:
            jsl_raise_msg("unexpected state type in PUSH callback");
    }
    if (!last_state) {
        jsn->data = (void *)val;
    } else if (last_state->type == JSONSL_T_LIST) {
        rb_ary_push(last_state->val, val);
    } else if (last_state->type == JSONSL_T_OBJECT) {
        Check_Type(last_state->val, T_HASH);
        if (state->type == JSONSL_T_HKEY) {
            last_state->pkey = val;
        } else {
            rb_hash_aset(last_state->val, last_state->pkey, val);
        }
    } else {
        jsl_raise_msg("unable to add value to non container type");
    }
    (void)action;
}

static VALUE jsl_jsonsl_parse(int argc, VALUE *argv, VALUE self)
{
    jsonsl_t jsn;
    VALUE nlevels = Qnil, str = Qnil;

    rb_scan_args(argc, argv, "11", &str, &nlevels);
    Check_Type(str, T_STRING);
    if (nlevels != Qnil) {
        Check_Type(nlevels, T_FIXNUM);
        jsn = jsonsl_new(FIX2INT(nlevels));
    } else {
        jsn = jsonsl_new(JSONSL_MAX_LEVELS);
    }
    jsonsl_reset(jsn);
    jsn->data = (void *)Qnil;
    jsonsl_enable_all_callbacks(jsn);
    jsn->action_callback_PUSH = jsl_jsonsl_push_callback;
    jsn->action_callback_POP = jsl_jsonsl_pop_callback;
    jsn->error_callback = jsl_jsonsl_error_callback;
    jsonsl_feed(jsn, RSTRING_PTR(str), RSTRING_LEN(str));
    if (jsn->level != 0) {
        jsl_raise_msg("unexpected end of data");
    }
    (void)self;
    return (VALUE)jsn->data;
}

void Init_jsonsl_ext()
{
    jsl_mJSONSL = rb_define_module("JSONSL");
    rb_define_const(jsl_mJSONSL, "REVISION", rb_str_freeze(rb_str_new_cstr(JSONSL_REVISION)));
    jsl_eError = rb_const_get(jsl_mJSONSL, rb_intern("Error"));
    rb_define_singleton_method(jsl_mJSONSL, "parse", jsl_jsonsl_parse, -1);
    jsl_row_parser_init();
}
