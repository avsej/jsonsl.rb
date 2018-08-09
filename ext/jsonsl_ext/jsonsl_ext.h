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

#include <ruby.h>

#include "jsonsl.h"

#ifndef JSONSL_EXT_H
#define JSONSL_EXT_H

extern VALUE jsl_mJSONSL;
extern VALUE jsl_eError;

void jsl_raise_at(jsonsl_error_t code, const char *message, const char *file, int line);
#define jsl_raise(code, message) jsl_raise_at(code, message, __FILE__, __LINE__)
#define jsl_raise_msg(message) jsl_raise_at(0, message, __FILE__, __LINE__)

void jsl_row_parser_init();

#endif
