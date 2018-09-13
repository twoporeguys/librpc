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

#ifndef LIBRPC_JSON_H
#define LIBRPC_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#define	JSON_EXTTYPE_UINT64	"$uint"
#define	JSON_EXTTYPE_DATE	"$date"
#define	JSON_EXTTYPE_BINARY	"$bin"
#define	JSON_EXTTYPE_FD		"$fd"
#define	JSON_EXTTYPE_ERROR	"$error"

#define	JSON_EXTTYPE_DBL	"$double"
#define	JSON_EXTTYPE_DBL_NAN	"$nan"
#define	JSON_EXTTYPE_DBL_INF	"$inf"
#define	JSON_EXTTYPE_DBL_NINF	"$ninf"

#define JSON_EXTTYPE_ERROR_CODE	"code"
#define JSON_EXTTYPE_ERROR_MSG	"msg"
#define JSON_EXTTYPE_ERROR_XTRA	"extra"
#define JSON_EXTTYPE_ERROR_STCK	"stack"

#if defined(__linux__)
#define	JSON_EXTTYPE_SHMEM	"$shmem"

#define JSON_EXTTYPE_SHMEM_ADDR "addr"
#define JSON_EXTTYPE_SHMEM_LEN	"len"
#define JSON_EXTTYPE_SHMEM_FD	"fd"
#endif

int rpc_json_serialize(rpc_object_t, void **, size_t *);
rpc_object_t rpc_json_deserialize(const void *, size_t);

#ifdef __cplusplus
}
#endif

#endif /* LIBRPC_JSON_H */
