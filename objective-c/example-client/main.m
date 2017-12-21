//
//  main.m
//  example-client
//
//  Created by Jakub Klama on 19.12.2017.
//  Copyright © 2017 Jakub Klama. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "librpc.h"

int main(int argc, const char * argv[]) {
    
    NSNumber *b = [NSNumber numberWithUnsignedInteger:5];
    RPCObject *a = [RPCObject initWithValue:b];
    @autoreleasepool {
        // insert code here...
        NSLog([a describe]);
        NSLog(@"%s", [b objCType]);
    }
    return 0;
}
