//
//  librpc.h
//  librpc
//
//  Created by Jakub Klama on 18.12.2017.
//  Copyright © 2017 Jakub Klama. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface RPCObject : NSObject
+ (RPCObject *)initWithValue:(id)value;
+ (RPCObject *)initFromNativeObject:(void *)object;
- (NSString *)describe;
- (NSObject *)value;
- (void *)nativeValue;
@end

@interface RPCCall : NSObject
+ (RPCCall *)initFromNativeObject:(void *)object;
- (void)wait;
- (void)resume;
- (void)abort;
- (RPCObject *)result;
@end

typedef void(^RPCFunctionCallback)(RPCCall *call, RPCObject *value);

@interface RPCClient : NSObject
- (void)connect:(NSString *)uri;
- (void)disconnect;
- (NSDictionary *)instances;
- (RPCCall *)call:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args;
- (RPCObject *)callSync:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args;
- (void)callAsync:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args callback:(RPCFunctionCallback)cb;
@end

@interface RPCInstance : NSObject
@property (readonly) RPCClient *client;
@property (readonly) NSString *path;
@end

@interface RPCInterface : NSObject
@property (readonly) RPCInstance *instance;
- (void)forwardInvocation:(NSInvocation *)anInvocation;
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector;
@end
