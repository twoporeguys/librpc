/*+
 * Copyright 2017 Two Pore Guys, Inc.
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

#import "librpc.h"
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/client.h>

@implementation RPCObject {
    rpc_object_t obj;
}

+ (RPCObject *)initWithValue:(id)value {
    RPCObject *result = [RPCObject alloc];
    
    if (value == nil) {
        result->obj = rpc_null_create();
        return (result);
    }
    
    if ([value isKindOfClass:[RPCObject class]]) {
        result->obj = rpc_retain([(RPCObject *)value nativeValue]);
        return (result);
    }

    if ([value isKindOfClass:[NSNumber class]]) {
        result->obj = rpc_int64_create([(NSNumber *)value integerValue]);
        return (result);
    }
    
    if ([value isKindOfClass:[NSString class]]) {
        result->obj = rpc_string_create([(NSString *)value UTF8String]);
        return (result);
    }
    
    if ([value isKindOfClass:[NSDate class]]) {
        
    }
    
    if ([value isKindOfClass:[NSData class]]) {
        
    }
    
    if ([value isKindOfClass:[NSException class]]) {
        result->obj = rpc_error_create(0, [[(NSException *)value reason] UTF8String], NULL);
        return (result);
    }
    
    if ([value isKindOfClass:[NSArray class]]) {
        result->obj = rpc_array_create();
        for (id object in (NSArray *)value) {
            RPCObject *robj = [RPCObject initWithValue:object];
            rpc_array_append_stolen_value(result->obj, robj->obj);
        }
        
        return (result);
    }
    
    if ([value isKindOfClass:[NSDictionary class]]) {
        result->obj = rpc_dictionary_create();
        for (NSString *key in (NSDictionary *)value) {
            NSObject *obj = [(NSDictionary *)value valueForKey:key];
            RPCObject *robj = [RPCObject initWithValue:obj];
            rpc_dictionary_set_value(result->obj, [key UTF8String], robj->obj);
        }
        
        return (result);
    }
    
    @throw [NSException exceptionWithName:@"foo" reason:nil userInfo:nil];
}

+ (instancetype)initWithValue:(id)value andType:(RPCType)type {
    RPCObject *result = [RPCObject alloc];

    switch (type) {
        case RPCTypeBoolean:
            result->obj = rpc_bool_create([(NSNumber *)value boolValue]);
            return result;

        case RPCTypeUInt64:
            result->obj = rpc_uint64_create([(NSNumber *)value unsignedIntegerValue]);
            return result;

        case RPCTypeInt64:
            result->obj = rpc_int64_create([(NSNumber *)value integerValue]);
            return result;

        case RPCTypeDouble:
            result->obj = rpc_double_create([(NSNumber *)value doubleValue]);
            return result;

        case RPCTypeFD:
            result->obj = rpc_fd_create([(NSNumber *)value integerValue]);
            return result;

        case RPCTypeNull:
        case RPCTypeString:
        case RPCTypeBinary:
        case RPCTypeArray:
        case RPCTypeDictionary:
            return [RPCObject initWithValue:value];

        default:
            return nil;
    }
}

+ (RPCObject *)initFromNativeObject:(void *)object {
    RPCObject *result = [RPCObject alloc];
    result->obj = rpc_retain(object);
    return result;
}

- (NSString *)describe {
    return ([[NSString alloc] initWithUTF8String:rpc_copy_description(obj)]);
}

- (NSObject *)value {
    __block NSMutableArray *array;
    __block NSMutableDictionary *dict;
    
    switch (rpc_get_type(obj)) {
        case RPC_TYPE_NULL:
            return (nil);
            
        case RPC_TYPE_BOOL:
            return [NSNumber numberWithBool:rpc_bool_get_value(obj)];
            
        case RPC_TYPE_INT64:
            return [NSNumber numberWithInteger:rpc_int64_get_value(obj)];
            
        case RPC_TYPE_UINT64:
            return [NSNumber numberWithUnsignedInteger:rpc_uint64_get_value(obj)];
            
        case RPC_TYPE_DOUBLE:
            return [NSNumber numberWithDouble:rpc_double_get_value(obj)];
        
        case RPC_TYPE_STRING:
            return [NSString stringWithUTF8String:rpc_string_get_string_ptr(obj)];
            
        case RPC_TYPE_DATE:
            return [NSDate dateWithTimeIntervalSince1970:rpc_date_get_value(obj)];
            
        case RPC_TYPE_BINARY:
            return [NSData dataWithBytes:rpc_data_get_bytes_ptr(obj) length:rpc_data_get_length(obj)];
            
        case RPC_TYPE_ARRAY:
            array = [[NSMutableArray alloc] init];
            rpc_array_apply(obj, ^bool(size_t index, rpc_object_t value) {
                [array addObject:[RPCObject initFromNativeObject:value]];
                return true;
            });
            
            return array;
            
        case RPC_TYPE_DICTIONARY:
            dict = [[NSMutableDictionary alloc] init];
            rpc_dictionary_apply(obj, ^bool(const char *key, rpc_object_t value) {
                [dict setObject:[RPCObject initFromNativeObject:value] forKey:[NSString stringWithUTF8String:key]];
                return true;
            });
            
            return dict;
            
        case RPC_TYPE_FD:
            return [NSNumber numberWithInteger:rpc_fd_get_value(obj)];
            
        case RPC_TYPE_ERROR:
            return nil;
    }
}

- (void *)nativeValue {
    return obj;
}

- (RPCType)type {
    return (RPCType)rpc_get_type(obj);
}
@end

@implementation RPCUnsignedInt
+ (instancetype)init:(NSNumber *)value {
    return [super initWithValue:value andType:RPCTypeUInt64];
}
@end

@implementation RPCDouble
+ (instancetype)init:(NSNumber *)value {
    return [super initWithValue:value andType:RPCTypeDouble];
}
@end

@implementation RPCBool
+ (instancetype)init:(NSNumber *)value {
    return [super initWithValue:value andType:RPCTypeBoolean];
}
@end

@implementation RPCCall {
    rpc_call_t call;
}

+ (RPCCall *)initFromNativeObject:(void *)object {
    RPCCall *result;
    
    result = [RPCCall alloc];
    result->call = object;
    return result;
}

- (void)wait {
    rpc_call_wait(call);
}

- (void)resume {
    rpc_call_continue(call, false);
}

- (void)abort {
    rpc_call_abort(call);
}

- (RPCObject *)result {
    return [RPCObject initFromNativeObject:rpc_call_result(call)];
}
@end

@implementation RPCClient {
    rpc_client_t client;
    rpc_connection_t conn;
}

- (void)connect:(NSString *)uri {
    client = rpc_client_create([uri UTF8String], NULL);
    conn = rpc_client_get_connection(client);
}

- (void)disconnect {
    
}

- (NSDictionary *)instances {
    return nil;
}

- (RPCObject *)callSync:(NSString *)method
                   path:(NSString *)path
              interface:(NSString *)interface
                   args:(RPCObject *)args {
    rpc_call_t call;
    
    call = rpc_connection_call(conn, [path UTF8String], [interface UTF8String], [method UTF8String], NULL, NULL);
    rpc_call_wait(call);
    
    switch (rpc_call_status(call)) {
        case RPC_CALL_DONE:
            return [RPCObject initFromNativeObject:rpc_call_result(call)];
            
        case RPC_CALL_ERROR:
            @throw [RPCObject initFromNativeObject:rpc_call_result(call)];
        
        case RPC_CALL_MORE_AVAILABLE:
            return nil;
        
        default:
            return nil;
    }
}

- (RPCCall *)call:(NSString *)method
             path:(NSString *)path
        interface:(NSString *)interface
             args:(RPCObject *)args {
    return [RPCCall initFromNativeObject:rpc_connection_call(conn, [path UTF8String], [interface UTF8String], [method UTF8String], NULL, NULL)];
    
}

- (void)callAsync:(NSString *)method
             path:(NSString *)path
        interface:(NSString *)interface
             args:(RPCObject *)args
         callback:(RPCFunctionCallback)cb {
    __block rpc_call_t call;
    
    call = rpc_connection_call(conn, [path UTF8String], [interface UTF8String],
                               [method UTF8String], [args nativeValue],
                               ^bool(rpc_object_t args, rpc_call_status_t status) {
        cb([RPCCall initFromNativeObject:call], [RPCObject initFromNativeObject:rpc_call_result(call)]);
        return (bool)true;
    });
}
@end
