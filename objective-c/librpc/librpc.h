//
//  librpc.h
//  librpc
//
//  Created by Jakub Klama on 18.12.2017.
//  Copyright © 2017 Jakub Klama. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface RPCObject : NSObject
+ (RPCObject *)initWithValue:(NSObject *)value;
- (NSString *)describe;
@end

@interface RPCUnsignedNumber : NSNumber
+ (void)initWithValue:(NSNumber *)value;
@end

@interface RPCBool : NSNumber
+ (void)initWithValue:(BOOL)value;
@end

@interface RPCClient : NSObject
- (void)connect:(NSString *)uri;
- (void)disconnect;
- (RPCObject *)callSync:(NSString *)method path:(NSString *)path interface:(NSString *)interface args:(RPCObject *)args;
@end

@interface RPCInstance : NSObject

@end

@interface RPCInterface : NSObject

@end
