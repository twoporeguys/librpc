//
//  main.swift
//  example-swift
//
//  Created by Jakub Klama on 19.12.2017.
//  Copyright © 2017 Jakub Klama. All rights reserved.
//

import Foundation
import librpc

var client = librpc.RPCClient();
client.connect("tcp://10.251.2.127:5001");
var result = client.callSync("ping", path: "/server", interface: "com.twoporeguys.momd.Builtin", args: nil);

client.callAsync("ping", path: "/server", interface: "com.twoporeguys.momd.Builtin", args: nil, callback: { (call: librpc.RPCCall, value: librpc.RPCObject) in
    print("async result", value.describe());
});
sleep(2);
print(result.describe());
var foo: UInt32 = 0xffffccff;
var a = librpc.RPCObject.initWithValue(["A":5, "b":true, "c":3.5, "d":foo]);
print(a.describe());

