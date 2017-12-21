//
//  librpc.h
//  librpc
//
//  Created by Jakub Klama on 18.12.2017.
//  Copyright © 2017 Jakub Klama. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface RPCObject : NSObject
+ (nonnull RPCObject *)initWithValue:(nullable id)value;
+ (nonnull RPCObject *)initFromNativeObject:(nullable void *)object;
- (nonnull NSString *)describe;
- (nonnull NSObject *)value;
- (nonnull void *)nativeValue;
@end

@interface RPCCall : NSObject
+ (nonnull RPCCall *)initFromNativeObject:(nonnull void *)object;
- (void)wait;
- (void)resume;
- (void)abort;
- (nullable RPCObject *)result;
@end

typedef void(^RPCFunctionCallback)(RPCCall * _Nonnull call, RPCObject * _Nonnull value);

@interface RPCClient : NSObject
- (void)connect:(nonnull NSString *)uri;
- (void)disconnect;
- (nonnull NSDictionary *)instances;
- (nonnull RPCCall *)call:(nonnull NSString *)method path:(nullable NSString *)path interface:(nullable NSString *)interface args:(nullable RPCObject *)args;
- (nonnull RPCObject *)callSync:(nonnull NSString *)method path:(nullable NSString *)path interface:(nullable NSString *)interface args:(nullable RPCObject *)args;
- (void)callAsync:(nonnull NSString *)method path:(nullable NSString *)path interface:(nullable NSString *)interface args:(nullable RPCObject *)args callback:(nonnull RPCFunctionCallback)cb;
@end

@interface RPCInstance : NSObject
@property (readonly, nonnull) RPCClient *client;
@property (readonly, nonnull) NSString *path;
@end

@interface RPCInterface : NSObject
@property (readonly, nonnull) RPCInstance *instance;
- (void)forwardInvocation:(nonnull NSInvocation *)anInvocation;
- (nonnull NSMethodSignature *)methodSignatureForSelector:(nonnull SEL)aSelector;
@end
