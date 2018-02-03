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
#import "librpc.h"

int main(int argc, const char * argv[]) {
    
    //NSNumber *b = [NSNumber numberWithUnsignedInteger:5];
    //RPCObject *a = [[RPCObject alloc] initWithValue:b];
    
    RPCClient *cl = [[RPCClient alloc] init];
    //
    [cl connect:@"ws://localhost:5000/ws"];
    NSDictionary *c = cl.instances;
    NSDictionary *d =  cl.spaInstances;
    RPCInstance *e = c[@"/module/task"];
    NSDictionary *ed = e.interfaces;
    //RPCInstance *spa = [[RPCInstance alloc] initWithClient:cl andPath:@"/spa/CharlieAssembly139"];
    RPCInstance *spa = d[@"/spa/CharlieAssembly139"];
    NSDictionary *sd = spa.interfaces;
    
    for (id key in c) {
        NSLog(@"Test 1: %@", key);
        if ([key isKindOfClass:[NSString class]]) {
            RPCInstance *inst = c[key];
            for (id key2 in inst.interfaces) {
                NSLog(@"________Test 2: %@", key2);
//                if ([key2 isKindOfClass:NSString.class]) {
//                    RPCInterface *interface = [[RPCInterface alloc] initWithClient:cl path:[NSString stringWithFormat:@"%@/%@", key, key2] andInterface:key2];
//                    if (interface) {
//                        NSDictionary *methods = interface.methods;
//                        NSLog(@"___________________Test 3:%@", methods);
//                    }
//                }
            }
        }
    }
    
    
    
    @autoreleasepool {
        // insert code here...
        //NSLog(@"%@\n%@", ed, sd);
        
//        [cl callAsync:@"query" path:@"/ds/CharlieAssembly139_adc" interface:@"com.twoporeguys.momd.DataSource" args:nil callback:^(RPCCall * _Nonnull call, RPCObject * _Nonnull value) {
//
//            NSLog(@"%@", [value value]);
//        }];
//        sleep(100000000);
    }
    return 0;
}
