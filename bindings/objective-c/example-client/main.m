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
    [cl connect:@"ws://localhost:5000/ws"];
    NSDictionary *instances = cl.instances;
    
    NSMutableDictionary *mInterfaces = [[NSMutableDictionary alloc] init];
    for (id key in cl.instances) {
        if ([key isKindOfClass:[NSString class]]) {
            RPCInstance *inst = cl.instances[key];
            [mInterfaces setObject:inst.interfaces forKey:key];
//            for (id key in inst.interfaces) {
//                
//            }
        }
    }
    RPCInterface *taskManager = mInterfaces[@"/task"][@"com.twoporeguys.momd.TaskManager"];
    RPCObject *nestedArgs = [[RPCObject alloc] initWithValue:@[@"/profile/generic", @"/spa/CharlieAssembly139"]];
    RPCObject *args = [[RPCObject alloc] initWithValue:@[@"test", nestedArgs]];
    RPCObject *node = [cl callSync:@"submit" path:taskManager.path interface:taskManager.interface args: args];
    
    for (id key in cl.instances) {
        if ([key isKindOfClass:[NSString class]]) {
            RPCInstance *inst = cl.instances[key];
            [mInterfaces setObject:inst.interfaces forKey:key];
        }
    }
    
    instances = cl.instances;
    RPCInterface *eventHandle = mInterfaces[@"/task/1"][@"com.twoporeguys.momd.ProfileTask"];
    NSArray *properties = [eventHandle properties];
    NSDictionary *pathDict = properties[3];
    NSString *path = [NSString stringWithFormat:@"/ds/%@", pathDict[@"value"][@"events"]];
    if (path) {
        [cl callAsync:@"query" path:path interface:@"com.twoporeguys.momd.DataSource" args:nil callback:^(RPCCall * _Nonnull call, RPCObject * _Nonnull value) {
        
            NSDictionary *dataDict = [value value];
            NSLog(@"callback: %@", dataDict);
//            NSData *data = [(RPCObject *)dataDict[@"samples"] value];
//            for (int i = 0; i < data.length / 4; i++) {
//                int32_t value = ((int32_t *)data.bytes)[i];
//                NSLog(@"value: %d", value);
//            }
        }];
    }
    
    @autoreleasepool {
        // insert code here...

        sleep(1000000000);
    }
    return 0;
}
