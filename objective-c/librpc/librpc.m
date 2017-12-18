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

+ (RPCObject *)initWithValue:(NSObject *)value {
    RPCObject *result = [RPCObject alloc];
    
    if (value == nil) {
        result->obj = rpc_null_create();
        return (result);
    }
    
    if ([value isKindOfClass:[NSNumber class]]) {
        result->obj = rpc_int64_create([(NSNumber *)value intValue]);
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
            NSObject *value = [(NSDictionary *)value valueForKey:key];
        }
        
        return (result);
    }
    
    @throw [NSException exceptionWithName:@"foo" reason:nil userInfo:nil];
}

- (NSString *)describe {
    return ([[NSString alloc] initWithUTF8String:rpc_copy_description(obj)]);
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

- (void)callSync:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args {
    rpc_call_t call;
    
    call = rpc_connection_call(conn, [path UTF8String], [interface UTF8String], [method UTF8String], NULL, NULL);
}
@end
