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

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <glib.h>
#include <rpc/connection.h>
#include <rpc/shmem.h>
#include "memfd.h"
#include "internal.h"

rpc_shmem_block_t
rpc_shmem_alloc(size_t size)
{
	rpc_shmem_block_t block;

	if (size == 0)
		return (NULL);

	block = g_malloc(sizeof(*block));
	block->rsb_addr = NULL;
	block->rsb_size = size;
	block->rsb_offset = 0;
	block->rsb_fd = memfd_create("librpc", 0);

	if (ftruncate(block->rsb_fd, size) != 0) {
		close(block->rsb_fd);
		g_free(block);
		return (NULL);
	}

	block->rsb_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
	    block->rsb_fd, 0);

	return (block);
}

void
rpc_shmem_free(rpc_shmem_block_t block)
{

	if (block == NULL)
		return;

	munmap(block->rsb_addr, block->rsb_size);
	close(block->rsb_fd);
	g_free(block);
}

void *
rpc_shmem_map(rpc_shmem_block_t block)
{

	return (mmap(NULL, block->rsb_size, PROT_READ | PROT_WRITE, MAP_SHARED,
	    block->rsb_fd, block->rsb_offset));
}

void *
rpc_shmem_block_get_ptr(rpc_shmem_block_t block)
{

	return (block->rsb_addr);
}

size_t
rpc_shmem_block_get_size(rpc_shmem_block_t block)
{

	return (block->rsb_size);
}

rpc_object_t
rpc_shmem_create(rpc_shmem_block_t block)
{
	union rpc_value val;

	val.rv_shmem = block;
	return (rpc_prim_create(RPC_TYPE_SHMEM, val));
}

rpc_shmem_block_t
rpc_shmem_get_block(rpc_object_t obj)
{

	if (rpc_get_type(obj) != RPC_TYPE_SHMEM)
		return (NULL);

	return (obj->ro_value.rv_shmem);
}
