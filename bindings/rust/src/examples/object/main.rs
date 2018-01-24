/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
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

#[macro_use]
extern crate maplit;
extern crate librpc;
use librpc::Create;
use librpc::{Value, Object, Client};
use std::thread;

fn main() {
    let a = Object::create("frob");
    let b = Object::new(Value::from("test"));
    let list = Object::create(&[Value::from(1i64), Value::from(2i64)][..]);
    let arr = Object::new(Value::from(vec![
        Value::from(-5i64),
        Value::from(5u64),
        Value::from(hashmap!{
            "hau" => Value::from(2i64),
            "test" => Value::from("foobar")
        })
    ]));

    println!("a = {:?}, b = {:?}", a, b);
    println!("list = {:?}", list);
    println!("arr = {:?}", arr);

    let client = Client::connect("ws://127.0.0.1:5000/ws");
    let conn = client.connection();
    let instance =

    conn.call_async("ping", "/server","com.twoporeguys.momd.Builtin", &[], Box::new(|call| {
        println!("result = {:?}", call.result());
        println!("state = {:?}", call.status());
        true
    }));

    thread::sleep(std::time::Duration::from_secs(2));
}
