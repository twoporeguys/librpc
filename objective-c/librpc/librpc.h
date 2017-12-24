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

@interface RPCObject : NSObject
+ (nonnull instancetype)initWithValue:(nullable id)value;
+ (nonnull instancetype)initWithValue:(nullable id)value andType:(RPCType)type;
+ (nonnull instancetype)initFromNativeObject:(nullable void *)object;
- (nonnull NSString *)describe;
- (nonnull NSObject *)value;
- (nonnull void *)nativeValue;
- (RPCType)type;
@end

@interface RPCUnsignedInt : RPCObject
+ (nonnull instancetype)init:(NSNumber *)value;
@end

@interface RPCBool : RPCObject
+ (nonnull instancetype)init:(NSNumber *)value;
@end

@interface RPCDouble : RPCObject
+ (nonnull instancetype)init:(NSNumber *)value;
@end

@interface RPCException : NSException
@end

@interface RPCCall : NSObject <NSFastEnumeration>
+ (nonnull RPCCall *)initFromNativeObject:(nonnull void *)object;
- (void)wait;
- (void)resume;
- (void)abort;
- (nullable RPCObject *)result;
- (NSUInteger)countByEnumeratingWithState:(nonnull NSFastEnumerationState *)state
                                  objects:(id _Nullable __unsafe_unretained [])buffer
                                    count:(NSUInteger)len;
@end

typedef void(^RPCFunctionCallback)(RPCCall * _Nonnull call, RPCObject * _Nonnull value);

@interface RPCClient : NSObject
- (void)connect:(nonnull NSString *)uri;
- (void)disconnect;
- (nonnull NSDictionary *)instances;
- (nonnull RPCCall *)call:(nonnull NSString *)method
                     path:(nullable NSString *)path
                interface:(nullable NSString *)interface
                     args:(nullable RPCObject *)args;
- (nonnull RPCObject *)callSync:(nonnull NSString *)method
                           path:(nullable NSString *)path
                      interface:(nullable NSString *)interface
                           args:(nullable RPCObject *)args;
- (void)callAsync:(nonnull NSString *)method
             path:(nullable NSString *)path
        interface:(nullable NSString *)interface
             args:(nullable RPCObject *)args
         callback:(nonnull RPCFunctionCallback)cb;
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
