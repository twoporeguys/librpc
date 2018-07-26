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

#ifndef LIBRPC_RPCD_H
#define LIBRPC_RPCD_H

/**
 * @file rpcd.h
 */

#define	RPCD_SOCKET_ENV		"RPCD_SOCKET_LOCATION"
#define	RPCD_SOCKET_LOCATION	"unix:///var/run/rpcd.sock"
#define	RPCD_MANAGER_INTERFACE	"com.twoporeguys.rpcd.ServiceManager"
#define	RPCD_SERVICE_INTERFACE	"com.twoporeguys.rpcd.Service"

/**
 * Looks up a service with given name and connects to it.
 *
 * @param service_name FQDN name of the service
 * @return RPC client handle or NULL in case of an error
 */
_Nullable rpc_client_t rpcd_connect_to(const char *_Nonnull service_name);

/**
 *
 * @param uri
 * @param name
 * @param description
 * @return
 */
int rpcd_register(const char *_Nonnull uri, const char *_Nonnull name,
    const char *_Nullable description);

#endif /* LIBRPC_RPCD_H */
