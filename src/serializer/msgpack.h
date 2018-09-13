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

#ifndef LIBRPC_MSGPACK_H
#define LIBRPC_MSGPACK_H

#ifdef __cplusplus
extern "C" {
#endif

#define MSGPACK_EXTTYPE_DATE	1
#define MSGPACK_EXTTYPE_FD	2
#define MSGPACK_EXTTYPE_SHMEM	3
#define MSGPACK_EXTTYPE_ERROR	4

#define	MSGPACK_SHMEM_FD	"fd"
#define	MSGPACK_SHMEM_OFFSET	"offset"
#define	MSGPACK_SHMEM_LEN	"len"

#define	MSGPACK_ERROR_CODE	"code"
#define	MSGPACK_ERROR_MESSAGE	"message"
#define	MSGPACK_ERROR_EXTRA	"extra"
#define	MSGPACK_ERROR_STACK	"stack"

int rpc_msgpack_serialize(rpc_object_t, void **, size_t *);
rpc_object_t rpc_msgpack_deserialize(const void *, size_t);

#ifdef __cplusplus
}
#endif

#endif /* LIBRPC_MSGPACK_H */
