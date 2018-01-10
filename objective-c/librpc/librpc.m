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
#include <dispatch/dispatch.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/client.h>
#include <rpc/service.h>

@implementation RPCObject {
    rpc_object_t obj;
}

- (instancetype)initWithValue:(id)value {

    if (value == nil) {
        obj = rpc_null_create();
        return (self);
    }
    
    if ([value isKindOfClass:[RPCObject class]]) {
        obj = rpc_retain([(RPCObject *)value nativeValue]);
        return (self);
    }

    if ([value isKindOfClass:[NSNumber class]]) {
        obj = rpc_int64_create([(NSNumber *)value integerValue]);
        return (self);
    }
    
    if ([value isKindOfClass:[NSString class]]) {
        obj = rpc_string_create([(NSString *)value UTF8String]);
        return (self);
    }
    
    if ([value isKindOfClass:[NSDate class]]) {
        
    }
    
    if ([value isKindOfClass:[NSData class]]) {
        obj = rpc_data_create([(NSData *)value bytes], [(NSData *)value length], NULL);
        return (self);
    }
    
    if ([value isKindOfClass:[NSException class]]) {
        obj = rpc_error_create(0, [[(NSException *)value reason] UTF8String], NULL);
        return (self);
    }
    
    if ([value isKindOfClass:[NSArray class]]) {
        obj = rpc_array_create();
        for (id object in (NSArray *)value) {
            RPCObject *robj = [[RPCObject alloc] initWithValue:object];
            rpc_array_append_stolen_value(obj, robj->obj);
        }
        
        return (self);
    }
    
    if ([value isKindOfClass:[NSDictionary class]]) {
        obj = rpc_dictionary_create();
        for (NSString *key in (NSDictionary *)value) {
            NSObject *val = [(NSDictionary *)value valueForKey:key];
            RPCObject *robj = [[RPCObject alloc] initWithValue:val];
            rpc_dictionary_set_value(obj, [key UTF8String], robj->obj);
        }
        
        return (self);
    }
    
    @throw [NSException exceptionWithName:@"foo" reason:nil userInfo:nil];
}

- (instancetype)initWithValue:(id)value andType:(RPCType)type {
    switch (type) {
        case RPCTypeBoolean:
            obj = rpc_bool_create([(NSNumber *)value boolValue]);
            return self;

        case RPCTypeUInt64:
            obj = rpc_uint64_create([(NSNumber *)value unsignedIntegerValue]);
            return self;

        case RPCTypeInt64:
            obj = rpc_int64_create([(NSNumber *)value integerValue]);
            return self;

        case RPCTypeDouble:
            obj = rpc_double_create([(NSNumber *)value doubleValue]);
            return self;

        case RPCTypeFD:
            obj = rpc_fd_create([(NSNumber *)value intValue]);
            return self;

        case RPCTypeNull:
        case RPCTypeString:
        case RPCTypeBinary:
        case RPCTypeArray:
        case RPCTypeDictionary:
            return [[RPCObject alloc] initWithValue:value];

        default:
            return nil;
    }
}

- (RPCObject *)initFromNativeObject:(void *)object {
    obj = rpc_retain(object);
    return self;
}

- (void)dealloc {
    rpc_release(obj);
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
                [array addObject:[[RPCObject alloc] initFromNativeObject:value]];
                return true;
            });
            
            return array;
            
        case RPC_TYPE_DICTIONARY:
            dict = [[NSMutableDictionary alloc] init];
            rpc_dictionary_apply(obj, ^bool(const char *key, rpc_object_t value) {
                [dict setObject:[[RPCObject alloc] initFromNativeObject:value] forKey:[NSString stringWithUTF8String:key]];
                return true;
            });
            
            return dict;
            
        case RPC_TYPE_FD:
            return [NSNumber numberWithInteger:rpc_fd_get_value(obj)];
            
        case RPC_TYPE_ERROR:
            return [[RPCException alloc] initWithCode:@(rpc_error_get_code(obj)) andMessage:@(rpc_error_get_message(obj))];
    }
}

- (void *)nativeValue {
    return obj;
}

- (RPCType)type {
    return (RPCType)rpc_get_type(obj);
}
@end

@implementation RPCException
- (nonnull instancetype)initWithCode:(NSNumber *)code andMessage:(NSString *)message {
    return (RPCException *)[NSException
            exceptionWithName:@"RPCException"
            reason:message
            userInfo:@{
                @"code": code,
                @"extra": [NSNull init],
                @"stack": [NSNull init]
            }];
}
@end

@implementation RPCUnsignedInt
- (instancetype)init:(NSNumber *)value {
    return [super initWithValue:value andType:RPCTypeUInt64];
}
@end

@implementation RPCDouble
- (instancetype)init:(NSNumber *)value {
    return [super initWithValue:value andType:RPCTypeDouble];
}
@end

@implementation RPCBool
- (instancetype)init:(BOOL)value {
    return [super initWithValue:@(value) andType:RPCTypeBoolean];
}
@end

@implementation RPCCall {
    rpc_call_t call;
}

- (instancetype)initFromNativeObject:(void *)object {
    call = object;
    return self;
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
    return [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)];
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(__unsafe_unretained id _Nullable [])buffer
                                    count:(NSUInteger)len {
    RPCObject *__autoreleasing tmp;
    rpc_call_continue(call, true);

    state->state = 1;
    state->mutationsPtr = (unsigned long *)call;
    state->itemsPtr = buffer;

    switch (rpc_call_status(call)) {
        case RPC_CALL_MORE_AVAILABLE:
            tmp = [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)];
            state->itemsPtr[0] = tmp;
            return (1);

        case RPC_CALL_ERROR:
            @throw [[[RPCObject alloc] initFromNativeObject:rpc_call_result(call)] value];

        case RPC_CALL_ENDED:
            return (0);
    }

    return (0);
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
    NSMutableDictionary *result = [[NSMutableDictionary alloc] init];
    RPCCall *call = [self call:@"get_instances" path:@"/" interface:@(RPC_DISCOVERABLE_INTERFACE) args:nil];

    for (RPCObject *value in call) {
        NSDictionary *item = (NSDictionary *)[value value];
        NSString *path = [[item objectForKey:@"path"] value];
        RPCInstance *instance = [[RPCInstance alloc] initWithClient:self andPath:path];
        [result setValue:instance forKey:path];
    }

    return result;
}

- (void)setDispatchQueue:(nullable dispatch_queue_t)queue {
    rpc_connection_set_dispatch_queue(conn, queue);
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
            return [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)];
            
        case RPC_CALL_ERROR:
            @throw [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)];
        
        case RPC_CALL_MORE_AVAILABLE:
        case RPC_CALL_ENDED:
            @throw [NSException exceptionWithName:@"RPCException" reason:@"Streaming RPC called with non-streaming API" userInfo:nil];
        
        default:
            NSAssert(true, @"Invalid RPC call state");
            return nil;
    }
}

- (RPCCall *)call:(NSString *)method
             path:(NSString *)path
        interface:(NSString *)interface
             args:(RPCObject *)args {
    rpc_call_t ret = rpc_connection_call(conn, [path UTF8String], [interface UTF8String], [method UTF8String], [args nativeValue], NULL);
    if (ret == NULL) {
        RPCObject *error = [[RPCObject alloc] initFromNativeObject:rpc_get_last_error()];
        @throw [error value];
    }
    return [[RPCCall alloc] initFromNativeObject:ret];
    
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
        cb([[RPCCall alloc] initFromNativeObject:call], [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)]);
        return (bool)true;
    });
}
@end

@implementation RPCInstance {
    RPCClient *client;
    NSString *path;
}

- (RPCClient *)client {
    return client;
}

- (NSString *)path {
    return path;
}

- (NSDictionary *)interfaces {
    NSMutableDictionary *result = [[NSMutableDictionary alloc] init];
    RPCCall *call = [client call:@"get_interfaces" path:path interface:@(RPC_INTROSPECTABLE_INTERFACE) args:nil];

    for (RPCObject *value in call) {
        NSString *name = (NSString *)[value value];
        RPCInterface *interface = [[RPCInterface alloc] initWithClient:client path:path andInterface:name];
        [result setValue:interface forKey:name];
    }

    return result;
}

- (instancetype)initWithClient:(RPCClient *)client andPath:(NSString *)path {
    self->client = client;
    self->path = path;
    return self;
}
@end

@implementation RPCInterface {
    RPCClient *client;
    NSString *path;
    NSString *interface;
}

- (instancetype)initWithClient:(RPCClient *)client path:(NSString *)path andInterface:(NSString *)interface {
    client = client;
    path = path;
    interface = interface;
    return self;
}

- (BOOL)respondsToSelector:(SEL)aSelector {
    return YES;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
    return [NSMethodSignature signatureWithObjCTypes:"@@"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    NSArray *args;
    RPCCall *result;
    [anInvocation getArgument:&args atIndex:0];
    result = [client call:@"" path:path interface:interface args:[[RPCObject alloc] initWithValue:args]];
    [anInvocation setReturnValue:(__bridge void *)result];
}
@end
