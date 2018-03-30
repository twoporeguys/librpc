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

#import <Foundation/Foundation.h>

@class RPCInstance;
@class RPCInterface;

typedef NS_ENUM(NSInteger, RPCType) {
    RPCTypeNull,
    RPCTypeBoolean,
    RPCTypeUInt64,
    RPCTypeInt64,
    RPCTypeDouble,
    RPCTypeString,
    RPCTypeBinary,
    RPCTypeFD,
    RPCTypeDictionary,
    RPCTypeArray,
    RPCTypeError
};

extern NSString * _Nullable const RPCErrorDomain;

/**
 * A boxed type representing librpc value.
 */
@interface RPCObject : NSObject
/**
 * Creates a new RPCObject from NSObject-based value.
 *
 * @param value Value
 * @returns Boxed RPCObject
 */
- (nonnull instancetype)initWithValue:(nullable id)value;

/**
 * Creates a new RPCObject from NSObject-based value with specific type.
 *
 * This function allows to override default type guess for the newly
 * created RPCObject. Useful when creating unsigned integer, floating point
 * or boolean values.
 *
 * @param value Value
 * @returns Boxed RPCObject
 */
- (nonnull instancetype)initWithValue:(nullable id)value andType:(RPCType)type;

/**
 * Initializes a new RPCObject from a C-based @ref rpc_object_t handle.
 *
 * @param object And @ref rpc_object_t handle
 * @returns Boxed RPCObject
 */
- (nonnull instancetype)initFromNativeObject:(nullable void *)object;

/**
 * Returns string description of the contained value.
 *
 * @returns RPCObject description
 */
- (nonnull NSString *)describe;
- (nullable id)value;
- (nonnull void *)nativeValue;
- (RPCType)type;
@end

@interface RPCUnsignedInt : RPCObject
- (nonnull instancetype)init:(nonnull NSNumber *)value;
@end

@interface RPCBool : RPCObject
- (nonnull instancetype)init:(BOOL)value;
@end

@interface RPCDouble : RPCObject
- (nonnull instancetype)init:(nonnull NSNumber *)value;
@end

@interface RPCError : NSObject
- (nonnull instancetype)initWithCode:(nonnull NSNumber *)code andMessage:(nonnull NSString *)message;
@end

@interface RPCCall : NSObject <NSFastEnumeration>
- (nonnull instancetype)initFromNativeObject:(nonnull void *)object;
- (void)wait;
- (void)resume;
- (void)abort;
- (nullable RPCObject *)result;
- (NSUInteger)countByEnumeratingWithState:(nonnull NSFastEnumerationState *)state
                                  objects:(id _Nullable __unsafe_unretained [_Nullable])buffer
                                    count:(NSUInteger)len;
@end

typedef void(^RPCFunctionCallback)(RPCCall * _Nonnull call, RPCObject * _Nonnull value);

typedef void(^RPCEventCallback)(RPCObject* _Nonnull value, NSString * _Nonnull path,
                                NSString * _Nonnull interface, NSString * _Nonnull method);

typedef void (^RPCPropertyCallback)(RPCObject *_Nonnull value);

@interface RPCClient : NSObject
/**
 * Connects to an endpoint specified by @p uri.
 *
 * @param uri URI of the endpoint to connect to
 */
- (BOOL)connect:(nonnull NSString *)uri error:(NSError * _Nullable *_Nullable)error;

/**
 * Terminates the current connection.
 *
 * If the instance is not connected, this call has no effect.
 */
- (void)disconnect;

/**
 * Returns a dictionary of instances found on the server.
 */
- (nonnull NSDictionary<NSString *, RPCInstance *> *)instances;
//- (void)setDispatchQueue:(nullable dispatch_queue_t)queue;

/**
 * Issues a call to the server.
 */
- (nonnull RPCCall *)call:(nonnull NSString *)method
                     path:(nullable NSString *)path
                interface:(nullable NSString *)interface
                     args:(nullable RPCObject *)args;

/**
 * Issues a call to the server and waits for the response.
 */
- (nullable RPCObject *)callSync:(nonnull NSString *)method
                           path:(nullable NSString *)path
                      interface:(nullable NSString *)interface
                           args:(nullable RPCObject *)args
                          error:(NSError * _Nullable *_Nullable)error;

/**
 * Issues a call to the server and runs @p callback when
 * there's response available.
 */
- (void)callAsync:(nonnull NSString *)method
             path:(nullable NSString *)path
        interface:(nullable NSString *)interface
             args:(nullable RPCObject *)args
         callback:(nonnull RPCFunctionCallback)cb;

/**
 * Waits for event, then fires callback
 * to act on event
 */
- (void)eventObserver:(nonnull NSString *)method
                 path:(nonnull NSString *)path
            interface:(nonnull NSString *)interface
             callback:(nullable RPCEventCallback)cb;

- (void)observeProperty:(nonnull NSString *)name
                   path:(nonnull NSString *)path
              interface:(nonnull NSString *)interface
               callback:(nullable RPCPropertyCallback)cb;

@end

@interface RPCInstance : NSObject
@property (readonly, nonnull) RPCClient *client;
@property (readonly, nonnull) NSString *path;
@property (readonly, nonnull) NSDictionary<NSString *, RPCInterface *> *interfaces;

- (nonnull instancetype)initWithClient:(nonnull RPCClient *)client andPath:(nonnull NSString *)path;
@end

@interface RPCInterface : NSObject
@property (readonly, nullable) RPCClient *client;
@property (readonly, nullable) NSString *path;
@property (readonly, nullable) NSString *interface;
@property (readonly, nullable) NSArray<NSDictionary *> *properties;
@property (readonly, nullable) NSArray<NSString *> *methods;
- (void)observeProperty:(nonnull NSString *)name callback:(nonnull RPCPropertyCallback)cb;
- (void)forwardInvocation:(nonnull NSInvocation *)anInvocation;
- (nonnull NSMethodSignature *)methodSignatureForSelector:(nonnull SEL)aSelector;
- (nonnull instancetype)initWithClient:(nonnull RPCClient *)client path:(nonnull NSString *)path andInterface:(nonnull NSString *)interface;
@end
