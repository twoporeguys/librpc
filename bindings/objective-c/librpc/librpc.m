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
#include <rpc/typing.h>

#pragma mark - RPCObject
@interface RPCObject ()
@property (nonatomic, readonly, unsafe_unretained) rpc_object_t obj;

@end

@implementation RPCObject

- (instancetype)initWithValue:(id)value
{
    self = [super init];
    if (self) {
        if (value == nil) {
            _obj = rpc_null_create();
        } else if ([value isKindOfClass:[RPCObject class]]) {
            _obj = rpc_retain([(RPCObject *)value nativeValue]);
        } else if ([value isKindOfClass:[NSNumber class]]) {
            if (strcmp([value objCType], @encode(BOOL)) == 0) {
                _obj = rpc_bool_create([(NSNumber *)value boolValue]);
            } else {
                _obj = rpc_int64_create([(NSNumber *)value integerValue]);
            }
        } else if ([value isKindOfClass:[NSString class]]) {
            _obj = rpc_string_create([(NSString *)value UTF8String]);
        } else if ([value isKindOfClass:[NSDate class]]) {
            _obj = rpc_date_create([(NSDate *)value timeIntervalSince1970]);
        } else if ([value isKindOfClass:[NSData class]]) {
            _obj = rpc_data_create([(NSData *)value bytes], [(NSData *)value length], NULL);
        } else if ([value isKindOfClass:[NSException class]]) {
            _obj = rpc_error_create(0, [[(NSException *)value reason] UTF8String], NULL);
        } else if ([value isKindOfClass:[NSArray class]]) {
            _obj = rpc_array_create();
            for (id object in (NSArray *)value) {
                RPCObject *robj = [[RPCObject alloc] initWithValue:object];
                rpc_array_append_value(_obj, robj->_obj);
            }
        } else if ([value isKindOfClass:[NSDictionary class]]) {
            _obj = rpc_dictionary_create();
            for (NSString *key in (NSDictionary *)value) {
                NSObject *val = [(NSDictionary *)value valueForKey:key];
                RPCObject *robj = [[RPCObject alloc] initWithValue:val];
                rpc_dictionary_set_value(_obj, [key UTF8String], robj->_obj);
            }
        } else {
            NSAssert(YES, @"Value does not correspond to any rpc_object classes");
                self = nil;
        }
    }
    return self;
}

- (instancetype)initWithValue:(id)value andType:(RPCType)type
{
    switch (type) {
        case RPCTypeBoolean:
            _obj = rpc_bool_create([(NSNumber *)value boolValue]);
            return self;
            
        case RPCTypeUInt64:
            _obj = rpc_uint64_create([(NSNumber *)value unsignedIntegerValue]);
            return self;
            
        case RPCTypeInt64:
            _obj = rpc_int64_create([(NSNumber *)value integerValue]);
            return self;
            
        case RPCTypeDouble:
            _obj = rpc_double_create([(NSNumber *)value doubleValue]);
            return self;
            
        case RPCTypeFD:
            _obj = rpc_fd_create([(NSNumber *)value intValue]);
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

- (RPCObject *)initFromNativeObject:(void *)object
{
    self = [super init];
    if (self) {
        if (object == NULL) {
            NSLog(@"Warning: Native RPCObject == NULL");
            return nil;
        }

        _obj = rpc_retain(object);
    }
    return self;
}

+ (nullable instancetype)lastError
{
    return [[RPCObject alloc] initFromNativeObject:rpc_get_last_error()];
}

- (void)dealloc
{
    rpc_release(_obj);
}

- (void)deleteRPCObject
{
    rpc_release(_obj);
}

- (NSString *)describe
{
    return ([[NSString alloc] initWithUTF8String:rpc_copy_description(_obj)]);
}

- (id)value
{
    __block NSMutableArray *array;
    __block NSMutableDictionary *dict;
    NSDictionary *userInfo;
    
    switch (rpc_get_type(_obj)) {
        case RPC_TYPE_NULL:
            return (nil);
            
        case RPC_TYPE_BOOL:
            return [NSNumber numberWithBool:rpc_bool_get_value(_obj)];
            
        case RPC_TYPE_INT64:
            return [NSNumber numberWithInteger:rpc_int64_get_value(_obj)];
            
        case RPC_TYPE_UINT64:
            return [NSNumber numberWithUnsignedInteger:rpc_uint64_get_value(_obj)];
            
        case RPC_TYPE_DOUBLE:
            return [NSNumber numberWithDouble:rpc_double_get_value(_obj)];
        
        case RPC_TYPE_STRING:
            return [NSString stringWithUTF8String:rpc_string_get_string_ptr(_obj)];
            
        case RPC_TYPE_DATE:
            return [NSDate dateWithTimeIntervalSince1970:rpc_date_get_value(_obj)];
            
        case RPC_TYPE_BINARY:
            return [NSData dataWithBytes:rpc_data_get_bytes_ptr(_obj)
                                  length:rpc_data_get_length(_obj)];
            
        case RPC_TYPE_ARRAY:
            array = [[NSMutableArray alloc] init];
            rpc_array_apply(_obj, ^bool(size_t index, rpc_object_t value) {
                [array addObject:[[RPCObject alloc] initFromNativeObject:value]];
                return true;
            });
            
            return array;
            
        case RPC_TYPE_DICTIONARY:
            dict = [[NSMutableDictionary alloc] init];
            rpc_dictionary_apply(_obj, ^bool(const char *key, rpc_object_t value) {
                [dict setObject:[[RPCObject alloc] initFromNativeObject:value]
                         forKey:[NSString stringWithUTF8String:key]];
                return true;
            });
            
            return dict;
            
        case RPC_TYPE_FD:
            return [NSNumber numberWithInteger:rpc_fd_get_value(_obj)];
            
        case RPC_TYPE_ERROR:
            userInfo = @{
                NSLocalizedDescriptionKey: @(rpc_error_get_message(_obj)),
                @"extra": [[RPCObject alloc] initFromNativeObject:rpc_error_get_extra(_obj)]
            };

            return [NSError errorWithDomain:NSPOSIXErrorDomain
                                       code:rpc_error_get_code(_obj)
                                   userInfo:userInfo];
    }
}

- (void *)nativeValue
{
    return _obj;
}

- (RPCType)type
{
    return (RPCType)rpc_get_type(_obj);
}
@end

#pragma mark - RPCTypes Exception UInt Double Bool

@implementation RPCUnsignedInt
- (instancetype)init:(NSNumber *)value
{
    return [super initWithValue:value andType:RPCTypeUInt64];
}
@end

@implementation RPCDouble
- (instancetype)init:(NSNumber *)value
{
    return [super initWithValue:value andType:RPCTypeDouble];
}
@end

@implementation RPCBool
- (instancetype)init:(BOOL)value
{
    return [super initWithValue:@(value) andType:RPCTypeBoolean];
}
@end

#pragma mark - RPCCall

@implementation RPCCall
{
    rpc_call_t call;
}

- (instancetype)initFromNativeObject:(void *)object
{
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

- (void)setPrefetch:(NSInteger)len {
    rpc_call_set_prefetch(call, (size_t)len);
}

- (RPCObject *)result {
    return [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)];
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(__unsafe_unretained id _Nullable [])buffer
                                    count:(NSUInteger)len
{
    RPCObject *__autoreleasing tmp;
    rpc_call_continue(call, true);
    if (rpc_call_status(call) == RPC_CALL_STREAM_START)
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

        case RPC_CALL_IN_PROGRESS:
            break;

        case RPC_CALL_STREAM_START:
        case RPC_CALL_ENDED:
        case RPC_CALL_DONE:
        case RPC_CALL_ABORTED:
            return (0);   
    }

    return (0);
}
@end

@interface RPCListenHandle (PrivateMethods)
- (instancetype)initWithConn:(rpc_connection_t)conn andCookie:(void *)cookie;
@end

@implementation RPCListenHandle
{
    rpc_connection_t _conn;
    void *_cookie;
}

- (instancetype)initWithConn:(rpc_connection_t)conn andCookie:(void *)cookie
{
    _conn = conn;
    _cookie = cookie;
    return self;
}

- (void)cancel
{
    rpc_connection_unregister_event_handler(_conn, _cookie);
}
@end

#pragma mark - RPCClient

@implementation RPCClient {
    rpc_client_t client;
    rpc_connection_t conn;
}

- (BOOL)connect:(NSString *)uri error:(NSError **)error
{
    client = rpc_client_create([uri UTF8String], NULL);
    if (client) {
        conn = rpc_client_get_connection(client);
        return YES;
    } else {
        if (error != nil)
            *error = [[RPCObject lastError] value];

        return NO;
    }
}

- (void *)nativeValue
{
    return client;
}

- (NSDictionary *)instances
{
    return [self instancesForPath:@"/"];
}

- (id)findInstance:(NSString *)name andInterface:(NSString *)interface
{
    RPCInstance *inst = [self.instances objectForKey:name];
    if (inst == nil)
        return nil;

    return [[inst interfaces] objectForKey:interface];
}

- (NSDictionary *)instancesForPath:(NSString *)path
{
    NSMutableDictionary *result = [[NSMutableDictionary alloc] init];
    NSError *error = nil;
    RPCObject *d = [self callSync:@"get_instances"
                             path:path
                        interface:@(RPC_DISCOVERABLE_INTERFACE)
                             args:nil
                            error:&error];
    
    for (RPCObject *i in [d value]) {
        NSDictionary *item = (NSDictionary *)[i value];
        NSString *iPath = [[item objectForKey:@"path"] value];
        RPCInstance *instance = [[RPCInstance alloc] initWithClient:self andPath:iPath];
        [result setValue:instance forKey:iPath];
    }
    return result;
}

- (void)setDispatchQueue:(nullable dispatch_queue_t)queue
{
    rpc_connection_set_dispatch_queue(conn, queue);
}

- (RPCObject *)callSync:(NSString *)method
                   path:(NSString *)path
              interface:(NSString *)interface
                   args:(RPCObject *)args
                  error:(NSError **)error
{
    NSDictionary *userInfo;
    rpc_call_t call;
    
    call = rpc_connection_call(conn, [path UTF8String], [interface UTF8String],
                               [method UTF8String], [args nativeValue], NULL);
    if (call == NULL) {
        if (error != nil)
            *error = [[RPCObject lastError] value];

        return (nil);
    }

    rpc_call_wait(call);

    switch (rpc_call_status(call)) {
        case RPC_CALL_DONE:
            return [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)];
            
        case RPC_CALL_ERROR:
            if (error != nil)
                *error = [[[RPCObject alloc] initFromNativeObject:rpc_call_result(call)] value];

            return nil;

        case RPC_CALL_STREAM_START:
        case RPC_CALL_MORE_AVAILABLE:
        case RPC_CALL_ENDED:
            if (error != nil) {
                userInfo = @{
                    NSLocalizedDescriptionKey: @"Streaming RPC called with non-streaming API"
                };

                *error = [NSError errorWithDomain:NSPOSIXErrorDomain
                                             code:EINVAL
                                         userInfo:userInfo];
            }

            return nil;

        default:
            NSAssert(true, @"Invalid RPC call state");
            return nil;
    }
}

- (RPCCall *)call:(NSString *)method
             path:(NSString *)path
        interface:(NSString *)interface
             args:(RPCObject *)args
            error:(NSError **)error
{
    rpc_call_t ret = rpc_connection_call(
        conn, [path UTF8String], [interface UTF8String], [method UTF8String],
        [args nativeValue], NULL);

    if (ret == NULL) {
        if (error != nil)
            *error = [[RPCObject lastError] value];

        return (nil);
    }

    return [[RPCCall alloc] initFromNativeObject:ret];
}

- (void)disconnect{
    rpc_client_close(client);
}

- (RPCCall *)callAsync:(NSString *)method
                  path:(NSString *)path
             interface:(NSString *)interface
                  args:(RPCObject *)args
              callback:(RPCFunctionCallback)cb
{
    __block rpc_call_t call;
    
    call = rpc_connection_call(conn, [path UTF8String], [interface UTF8String],
                               [method UTF8String], [args nativeValue],
                               ^bool(rpc_call_t call) {
        if (rpc_call_status(call) == RPC_CALL_STREAM_START)
            return (bool)true;

        cb([[RPCCall alloc] initFromNativeObject:call],
           [[RPCObject alloc] initFromNativeObject:rpc_call_result(call)]);
        return (bool)true;
    });
    return [[RPCCall alloc] initFromNativeObject:call];
}

- (nonnull RPCListenHandle *)eventObserver:(NSString *)method
                 path:(NSString *)path
            interface:(NSString *)interface
             callback:(RPCEventCallback)cb
{
    void *cookie;

    cookie = rpc_connection_register_event_handler(conn,
        [path UTF8String], [interface UTF8String], [method UTF8String],
        ^(const char *pathReturn, const char *interfaceReturn,
          const char *methodReturn, rpc_object_t args) {
            cb([[RPCObject alloc] initFromNativeObject: args],
               [[NSString alloc] initWithString:@(pathReturn)],
               [[NSString alloc] initWithString:@(interfaceReturn)],
               [[NSString alloc] initWithString:@(methodReturn)]);
        });

    NSAssert(cookie != NULL, @"rpc_connection_register_event_handler() failure");
    return [[RPCListenHandle alloc] initWithConn:conn andCookie:cookie];
}

- (nonnull RPCListenHandle *)observeProperty:(NSString *)name
                   path:(NSString *)path
              interface:(NSString *)interface
               callback:(RPCPropertyCallback)cb
{
    void *cookie;

    cookie = rpc_connection_watch_property(conn, [path UTF8String], [interface UTF8String],
                                           [name UTF8String], ^(rpc_object_t v) {
        cb([[RPCObject alloc] initFromNativeObject:v]);
    });

    NSAssert(cookie != NULL, @"rpc_connection_watch_property() failure");
    return [[RPCListenHandle alloc] initWithConn:conn andCookie:cookie];
}
@end

#pragma mark - RPCInstance

@implementation RPCInstance
{
    RPCClient *client;
    NSString *path;
}

- (RPCClient *)client
{
    return client;
}

- (NSString *)path
{
    return path;
}

- (NSDictionary *)interfaces {
    NSMutableDictionary *result = [[NSMutableDictionary alloc] init];
    NSError *error = nil;
    RPCObject *call = [client callSync:@"get_interfaces"
                                  path:path
                             interface:@(RPC_INTROSPECTABLE_INTERFACE)
                                  args:nil
                                 error:&error];

    for (RPCObject *value in [call value]) {
        NSString *name = (NSString *)[value value];
        RPCInterface *interface = [[RPCInterface alloc] initWithClient:client
                                                                  path:path
                                                          andInterface:name];

        [result setValue:interface forKey:name];
    }

    return result;
}

- (instancetype)initWithClient:(RPCClient *)client andPath:(NSString *)path
{
    self->client = client;
    self->path = path;
    return self;
}
@end

#pragma mark - RPCInterface

@implementation RPCInterface

- (instancetype)initWithClient:(RPCClient *)client
                          path:(NSString *)path
                  andInterface:(NSString *)interface
{
    self = [super init];
    if (self) {
        _client = client;
        _path = path;
        _interface = interface;
    }
    return self;
}

- (NSArray *)properties
{
    NSError *error = nil;
    NSMutableArray *result = [[NSMutableArray alloc] init];
    RPCObject *args = [[RPCObject alloc] initWithValue:@[_interface]];
    RPCObject *i = [_client callSync:@"get_all"
                                path:_path
                           interface:@(RPC_OBSERVABLE_INTERFACE)
                                args:args
                               error:&error];

    for (RPCObject *value in [i value])
        [result addObject:[self recursivelyUnpackProperties:value]];

    return result.copy;
}

- (nonnull RPCListenHandle *)observeProperty:(NSString *)name callback:(RPCPropertyCallback)cb
{
    return [_client observeProperty:name path:_path interface:_interface callback:cb];
}

- (id)recursivelyUnpackProperties:(id)container
{
    
    id tContainer = [container value];
    if ([tContainer isKindOfClass:NSDictionary.class]) {
        NSMutableDictionary *retDict = [NSMutableDictionary new];
        for (id key in tContainer) {
            id obj = [tContainer[key] value];
            if ([obj isKindOfClass:NSDictionary.class] || [obj isKindOfClass:NSArray.class]) {
                id uObj = [self recursivelyUnpackProperties:tContainer[key]];
                [retDict setObject:uObj forKey:key];
            } else {
                if (obj) [retDict setObject:obj forKey:key];
            }
        }
        return retDict;
    } else if ([tContainer isKindOfClass:NSArray.class]) {
        NSMutableArray *retArray = [NSMutableArray new];
        for (id cObj in tContainer) {
            id obj = [cObj value];
            if ([obj isKindOfClass:NSDictionary.class] || [obj isKindOfClass:NSArray.class]) {
                id uObj = [self recursivelyUnpackProperties:cObj];
                [retArray addObject:uObj];
            } else {
                if (obj)[retArray addObject:obj];
            }
        }
        return retArray;
    } else {
        return [tContainer value];
    }
}

- (NSArray *)methods
{
    NSMutableArray *result = [[NSMutableArray alloc] init];
    NSError *callError = nil;
    RPCObject *obj;
    
    obj = [_client callSync:@"get_methods"
                       path:_path interface:@(RPC_INTROSPECTABLE_INTERFACE)
                       args:[[RPCObject alloc] initWithValue:@[_interface]]
                      error:&callError];
    
    if (!result)
        @throw callError;
    
    for (RPCObject *value in [obj value]) {
        NSString *name = (NSString *)[value value];
        [result addObject:name];
    }
    
    return result;
}

- (id)call:(NSString *)method
      args:(NSObject *)args
     error:(NSError *__autoreleasing  _Nullable *)error
{
    return [_client callSync:method
                        path:_path
                   interface:_interface
                        args:[[RPCObject alloc]initWithValue:args]
                       error:error];
}

- (id)call:(NSString *)method error:(NSError *__autoreleasing  _Nullable *)error
{
    return [self call:method args:@[] error:error];
}

- (id)get:(NSString *)property error:(NSError *__autoreleasing _Nullable *)error
{
    return [_client callSync:@"get"
                        path:_path
                   interface:@(RPC_OBSERVABLE_INTERFACE)
                        args:[[RPCObject alloc] initWithValue:@[_interface, property]]
                       error:error];
}

- (id)set:(NSString *)property
    value:(NSObject *)value
    error:(NSError *__autoreleasing _Nullable *)error
{
    return [_client callSync:@"set"
                        path:_path
                   interface:@(RPC_OBSERVABLE_INTERFACE)
                        args:[[RPCObject alloc] initWithValue:@[_interface, property, value]]
                       error:error];
}
@end

@implementation RPCTyping

+ (instancetype)shared {
    static RPCTyping *rpcTyping = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        rpcTyping = [[self alloc] initPrivate];
    });
    return rpcTyping;
}

- (instancetype)init {
    return nil;
}

- (instancetype)initPrivate
{
    self = [super init];
    if (self) {
        rpct_init(true);
    }
    return self;
}

- (BOOL)loadTypes:(NSString *)path error:(NSError **)error
{
    if (rpct_load_types([path UTF8String]) != 0) {
        if (error != nil)
            *error = [[RPCObject lastError] value];

        return NO;
    }

    return YES;
}

- (BOOL)loadTypesDirectory:(NSString *)directory error:(NSError **)error
{
    if (rpct_load_types_dir([directory UTF8String]) != 0) {
        if (error != nil)
            *error = [[RPCObject lastError] value];

        return NO;
    }

    return YES;
}

- (BOOL)loadTypesConnection:(RPCClient *)client error:(NSError **)error
{
    rpc_connection_t conn;

    conn = rpc_client_get_connection([client nativeValue]);
    if (rpct_download_idl(conn) != 0) {
        if (error != nil)
            *error = [[RPCObject lastError] value];

        return NO;
    }

    return YES;
}

@end
