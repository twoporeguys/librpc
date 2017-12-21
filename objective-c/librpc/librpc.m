//
//  librpc.m
//  librpc
//
//  Created by Jakub Klama on 18.12.2017.
//  Copyright © 2017 Jakub Klama. All rights reserved.
//

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
    
    if ([value isKindOfClass:[NSNumber class]]) {
        NSNumber *number = (NSNumber *)value;
        
        /* XXX: doesn't work :( */
        switch ([number objCType][0]) {
            case 'c':
            case 's':
            case 'i':
            case 'q':
                result->obj = rpc_int64_create([number integerValue]);
                return (result);
            
            case 'C':
            case 'S':
            case 'I':
            case 'Q':
                result->obj = rpc_uint64_create([number unsignedIntegerValue]);
                return (result);
                
            case 'f':
            case 'd':
                result->obj = rpc_double_create([number doubleValue]);
                return (result);
                
            case 'B':
                result->obj = rpc_bool_create([number boolValue]);
                return (result);
        }
        
        @throw [NSException exceptionWithName:@"Invalid Value" reason:nil userInfo:nil];
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

- (RPCObject *)callSync:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args {
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

- (RPCCall *)call:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args {
    return [RPCCall initFromNativeObject:rpc_connection_call(conn, [path UTF8String], [interface UTF8String], [method UTF8String], NULL, NULL)];
    
}

- (void)callAsync:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args callback:(RPCFunctionCallback)cb {
    __block rpc_call_t call;
    
    call = rpc_connection_call(conn, [path UTF8String], [interface UTF8String], [method UTF8String], [args nativeValue], ^bool(rpc_object_t args, rpc_call_status_t status) {
        cb([RPCCall initFromNativeObject:call], [RPCObject initFromNativeObject:rpc_call_result(call)]);
        return (bool)true;
    });
}
@end
