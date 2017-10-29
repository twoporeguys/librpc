/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LIBRPC_SERIALIZER_H
#define LIBRPC_SERIALIZER_H

#include <rpc/object.h>

/**
 * @file serializer.h
 */

/**
 * Checks whether specified serializer is available.
 *
 * @param serializer Serializer name
 * @return true if specified serializer exists, otherwise false
 */
bool rpc_serializer_exists(const char *serializer);

/**
 * Loads an RPC object from a serialized blob.
 *
 * @param serializer Serializer type (msgpack, json or yaml)
 * @param frame Blob pointer
 * @param len Blob length
 * @return RPC object or NULL in case of error.
 */
rpc_object_t rpc_serializer_load(const char *serializer, const void *frame,
    size_t len);

/**
 * Dumps an RPC object into a serialized blob form.
 * @param serializer Serializer type (msgpack, json or yaml)
 * @param framep Pointer to a variable holding blob pointer
 * @param lenp Pointer to a variable holding resulting blob length
 * @return 0 on success, -1 on error
 */
int rpc_serializer_dump(const char *serializer, rpc_object_t obj,
    void **framep, size_t *lenp);

#endif //LIBRPC_SERIALIZER_H
