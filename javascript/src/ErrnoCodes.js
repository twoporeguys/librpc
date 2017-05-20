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

const errno = new Map([
    [1, {"name": "EPERM", "description": "Operation not permitted "}],
    [2, {"name": "ENOENT", "description": "No such file or directory "}],
    [3, {"name": "ESRCH", "description": "No such process "}],
    [4, {"name": "EINTR", "description": "Interrupted system call "}],
    [5, {"name": "EIO", "description": "Input/output error "}],
    [6, {"name": "ENXIO", "description": "Device not configured "}],
    [7, {"name": "E2BIG", "description": "Argument list too long "}],
    [8, {"name": "ENOEXEC", "description": "Exec format error "}],
    [9, {"name": "EBADF", "description": "Bad file descriptor "}],
    [10, {"name": "ECHILD", "description": "No child processes "}],
    [11, {"name": "EDEADLK", "description": "Resource deadlock avoided "}],
    [12, {"name": "ENOMEM", "description": "Cannot allocate memory "}],
    [13, {"name": "EACCES", "description": "Permission denied "}],
    [14, {"name": "EFAULT", "description": "Bad address "}],
    [15, {"name": "ENOTBLK", "description": "Block device required "}],
    [16, {"name": "EBUSY", "description": "Device busy "}],
    [17, {"name": "EEXIST", "description": "File exists "}],
    [18, {"name": "EXDEV", "description": "Cross-device link "}],
    [19, {"name": "ENODEV", "description": "Operation not supported by device "}],
    [20, {"name": "ENOTDIR", "description": "Not a directory "}],
    [21, {"name": "EISDIR", "description": "Is a directory "}],
    [22, {"name": "EINVAL", "description": "Invalid argument "}],
    [23, {"name": "ENFILE", "description": "Too many open files in system "}],
    [24, {"name": "EMFILE", "description": "Too many open files "}],
    [25, {"name": "ENOTTY", "description": "Inappropriate ioctl for device "}],
    [26, {"name": "ETXTBSY", "description": "Text file busy "}],
    [27, {"name": "EFBIG", "description": "File too large "}],
    [28, {"name": "ENOSPC", "description": "No space left on device "}],
    [29, {"name": "ESPIPE", "description": "Illegal seek "}],
    [30, {"name": "EROFS", "description": "Read-only filesystem "}],
    [31, {"name": "EMLINK", "description": "Too many links "}],
    [32, {"name": "EPIPE", "description": "Broken pipe "}],
    [33, {"name": "EDOM", "description": "Numerical argument out of domain "}],
    [34, {"name": "ERANGE", "description": "Result too large "}],
    [35, {"name": "EAGAIN", "description": "Resource temporarily unavailable "}],
    [36, {"name": "EINPROGRESS", "description": "Operation now in progress "}],
    [37, {"name": "EALREADY", "description": "Operation already in progress "}],
    [38, {"name": "ENOTSOCK", "description": "Socket operation on non-socket "}],
    [39, {"name": "EDESTADDRREQ", "description": "Destination address required "}],
    [40, {"name": "EMSGSIZE", "description": "Message too long "}],
    [41, {"name": "EPROTOTYPE", "description": "Protocol wrong type for socket "}],
    [42, {"name": "ENOPROTOOPT", "description": "Protocol not available "}],
    [43, {"name": "EPROTONOSUPPORT", "description": "Protocol not supported "}],
    [44, {"name": "ESOCKTNOSUPPORT", "description": "Socket type not supported "}],
    [45, {"name": "EOPNOTSUPP", "description": "Operation not supported "}],
    [46, {"name": "EPFNOSUPPORT", "description": "Protocol family not supported "}],
    [47, {"name": "EAFNOSUPPORT", "description": "Address family not supported by protocol family "}],
    [48, {"name": "EADDRINUSE", "description": "Address already in use "}],
    [49, {"name": "EADDRNOTAVAIL", "description": "Can't assign requested address "}],
    [50, {"name": "ENETDOWN", "description": "Network is down "}],
    [51, {"name": "ENETUNREACH", "description": "Network is unreachable "}],
    [52, {"name": "ENETRESET", "description": "Network dropped connection on reset "}],
    [53, {"name": "ECONNABORTED", "description": "Software caused connection abort "}],
    [54, {"name": "ECONNRESET", "description": "Connection reset by peer "}],
    [55, {"name": "ENOBUFS", "description": "No buffer space available "}],
    [56, {"name": "EISCONN", "description": "Socket is already connected "}],
    [57, {"name": "ENOTCONN", "description": "Socket is not connected "}],
    [58, {"name": "ESHUTDOWN", "description": "Can't send after socket shutdown "}],
    [59, {"name": "ETOOMANYREFS", "description": "Too many references: can't splice "}],
    [60, {"name": "ETIMEDOUT", "description": "Operation timed out "}],
    [61, {"name": "ECONNREFUSED", "description": "Connection refused "}],
    [62, {"name": "ELOOP", "description": "Too many levels of symbolic links "}],
    [63, {"name": "ENAMETOOLONG", "description": "File name too long "}],
    [64, {"name": "EHOSTDOWN", "description": "Host is down "}],
    [65, {"name": "EHOSTUNREACH", "description": "No route to host "}],
    [66, {"name": "ENOTEMPTY", "description": "Directory not empty "}],
    [67, {"name": "EPROCLIM", "description": "Too many processes "}],
    [68, {"name": "EUSERS", "description": "Too many users "}],
    [69, {"name": "EDQUOT", "description": "Disc quota exceeded "}],
    [70, {"name": "ESTALE", "description": "Stale NFS file handle "}],
    [71, {"name": "EREMOTE", "description": "Too many levels of remote in path "}],
    [72, {"name": "EBADRPC", "description": "RPC struct is bad "}],
    [73, {"name": "ERPCMISMATCH", "description": "RPC version wrong "}],
    [74, {"name": "EPROGUNAVAIL", "description": "RPC prog. not avail "}],
    [75, {"name": "EPROGMISMATCH", "description": "Program version wrong "}],
    [76, {"name": "EPROCUNAVAIL", "description": "Bad procedure for program "}],
    [77, {"name": "ENOLCK", "description": "No locks available "}],
    [78, {"name": "ENOSYS", "description": "Function not implemented "}],
    [79, {"name": "EFTYPE", "description": "Inappropriate file type or format "}],
    [80, {"name": "EAUTH", "description": "Authentication error "}],
    [81, {"name": "ENEEDAUTH", "description": "Need authenticator "}],
    [82, {"name": "EIDRM", "description": "Identifier removed "}],
    [83, {"name": "ENOMSG", "description": "No message of desired type "}],
    [84, {"name": "EOVERFLOW", "description": "Value too large to be stored in data type "}],
    [85, {"name": "ECANCELED", "description": "Operation canceled "}],
    [86, {"name": "EILSEQ", "description": "Illegal byte sequence "}],
    [87, {"name": "ENOATTR", "description": "Attribute not found "}],
    [88, {"name": "EDOOFUS", "description": "Programming error "}],
    [89, {"name": "EBADMSG", "description": "Bad message "}],
    [90, {"name": "EMULTIHOP", "description": "Multihop attempted "}],
    [91, {"name": "ENOLINK", "description": "Link has been severed "}],
    [92, {"name": "EPROTO", "description": "Protocol error "}],
    [93, {"name": "ENOTCAPABLE", "description": "Capabilities insufficient "}],
    [94, {"name": "ECAPMODE", "description": "Not permitted in capability mode "}],
    [95, {"name": "ENOTRECOVERABLE", "description": "State not recoverable "}],
    [96, {"name": "EOWNERDEAD", "description": "Previous owner died "}],
]);


export function getErrno(code)
{
    return errno.get(code);
}

export function getCode(name)
{
    for (let [k, v] of errno.entries()) {
        if (v.name == name)
            return {
                "code": k,
                "name": v.name,
                "description": v.description
            };
    }

    return null;
}
