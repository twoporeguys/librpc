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

extern crate libc;
extern crate block;
use std::fmt;
use std::ffi::{CString, CStr};
use std::collections::hash_map::HashMap;
use std::os::raw::{c_char, c_void};
use std::ptr::{null, null_mut};
use std::mem::transmute;
use std::cell::RefCell;
use std::rc::Weak;
use libc::free;
use block::{Block, ConcreteBlock};

macro_rules! to_cstr {
    ($e:expr) => (CString::new($e).unwrap())
}

macro_rules! null_block {
    () => (transmute::<*mut c_void, _>(null_mut()))
}

#[repr(C)]
#[derive(Debug)]
pub enum RawType
{
    Null,
    Bool,
    Uint64,
    Int64,
    Double,
    Date,
    String,
    Binary,
    Fd,
    Dictionary,
    Array,
    Error,
}

#[repr(C)]
#[derive(Debug)]
pub enum CallStatus
{
    InProgress,
    MoreAvailable,
    Done,
    Error,
    Aborted,
    Ended
}

pub enum RawObject {}
pub enum RawConnection {}
pub enum RawClient {}
pub enum RawCall {}

pub struct Object
{
    value: *mut RawObject,
}

pub struct Connection
{
    value: *mut RawConnection
}

pub struct Client
{
    value: *mut RawClient,
    connection: Connection
}

pub struct Call<'a>
{
    connection: &'a Connection,
    value: *mut RawCall
}

pub struct Instance<'a>
{
    connection: &'a Connection,
    path: String
}

pub struct Interface<'a>
{
    instance: &'a Instance<'a>,
    name: String
}

#[derive(Clone, Debug)]
pub enum Value
{
    Null,
    Bool(bool),
    Uint64(u64),
    Int64(i64),
    Double(f64),
    Date(u64),
    String(String),
    Binary(Vec<u8>),
    Array(Vec<Value>),
    Dictionary(HashMap<String, Value>),
    Object(Object),
    Fd(i32),
    Error(Error)
}

#[derive(Clone, Debug)]
pub struct Error
{
    code: u32,
    message: String,
    stack_trace: Box<Value>,
    extra: Box<Value>
}

#[link(name = "rpc")]
extern {
    /* rpc/object.h */
    pub fn rpc_get_type(value: *mut RawObject) -> RawType;
    pub fn rpc_hash(value: *mut RawObject) -> u32;
    pub fn rpc_null_create() -> *mut RawObject;
    pub fn rpc_bool_create(value: bool) -> *mut RawObject;
    pub fn rpc_bool_get_value(value: *mut RawObject) -> bool;
    pub fn rpc_uint64_create(value: u64) -> *mut RawObject;
    pub fn rpc_uint64_get_value(value: *mut RawObject) -> u64;
    pub fn rpc_int64_create(value: i64) -> *mut RawObject;
    pub fn rpc_int64_get_value(value: *mut RawObject) -> i64;
    pub fn rpc_double_create(value: f64) -> *mut RawObject;
    pub fn rpc_double_get_value(value: *mut RawObject) -> f64;
    pub fn rpc_date_create(value: u64) -> *mut RawObject;
    pub fn rpc_date_get_value(obj: *mut RawObject) -> u64;
    pub fn rpc_string_create(value: *const c_char) -> *mut RawObject;
    pub fn rpc_string_get_string_ptr(value: *mut RawObject) -> *const c_char;
    pub fn rpc_data_create(ptr: *const u8, len: usize, dtor: *const c_void) -> *mut RawObject;
    pub fn rpc_array_create() -> *mut RawObject;
    pub fn rpc_dictionary_create() -> *mut RawObject;
    pub fn rpc_array_append_value(obj: *mut RawObject, value: *mut RawObject);
    pub fn rpc_dictionary_set_value(obj: *mut RawObject, key: *const c_char, value: *mut RawObject);
    pub fn rpc_fd_create(value: i32) -> *mut RawObject;
    pub fn rpc_fd_get_value(obj: *mut RawObject) -> i32;
    pub fn rpc_copy_description(value: *mut RawObject) -> *mut c_char;
    pub fn rpc_retain(value: *mut RawObject) -> *mut RawObject;
    pub fn rpc_release_impl(value: *mut RawObject);

    /* rpc/connection.h */
    pub fn rpc_connection_call(conn: *mut RawConnection, path: *const c_char,
                               interface: *const c_char, name: *const c_char,
                               args: *const RawObject,
                               callback: &Block<(*mut RawCall,), bool>) -> *mut RawCall;

    pub fn rpc_call_status(call: *mut RawCall) -> CallStatus;
    pub fn rpc_call_result(call: *mut RawCall) -> *mut RawObject;
    pub fn rpc_call_continue(call: *mut RawCall);
    pub fn rpc_call_abort(call: *mut RawCall);
    pub fn rpc_call_wait(call: *mut RawCall);

    /* rpc/client.h */
    pub fn rpc_client_create(uri: *const c_char, params: *const RawObject) -> *mut RawClient;
    pub fn rpc_client_get_connection(client: *mut RawClient) -> *mut RawConnection;
}

pub trait Create<T> {
    fn create(value: T) -> Object;
}

impl Clone for Object {
    fn clone(&self) -> Object {
        unsafe {
            return Object { value: rpc_retain(self.value) }
        }
    }
}

impl Drop for Object {
    fn drop(&mut self) {
        unsafe {
            rpc_release_impl(self.value)
        }
    }
}

impl<T> Create<T> for Object where Value: std::convert::From<T> {
    fn create(value: T) -> Object {
        Object::new(Value::from(value))
    }
}

impl From<bool> for Value {
    fn from(value: bool) -> Value {
        Value::Bool(value)
    }
}

impl From<u64> for Value {
    fn from(value: u64) -> Value {
        Value::Uint64(value)
    }
}

impl From<i64> for Value {
    fn from(value: i64) -> Value {
        Value::Int64(value)
    }
}

impl From<f64> for Value {
    fn from(value: f64) -> Value {
        Value::Double(value)
    }
}

impl<'a> From<&'a str> for Value {
    fn from(value: &str) -> Value {
        Value::String(String::from(value))
    }
}

impl From<String> for Value {
    fn from(value: String) -> Value {
        Value::String(value)
    }
}

impl From<Vec<u8>> for Value {
    fn from(value: Vec<u8>) -> Value {
        Value::Binary(value)
    }
}

impl<'a> From<&'a [Value]> for Value {
    fn from(value: &[Value]) -> Value {
        Value::Array(value.to_vec())
    }
}

impl From<Vec<Value>> for Value {
    fn from(value: Vec<Value>) -> Value {
        Value::Array(value)
    }
}

impl<'a> From<HashMap<&'a str, Value>> for Value {
    fn from(value: HashMap<&str, Value>) -> Value {
        Value::Dictionary(value.iter().map( | ( & k, v) |
            (String::from(k), v.clone())
        ).collect())
    }
}

impl From<HashMap<String, Value>> for Value {
    fn from(value: HashMap<String, Value>) -> Value {
        Value::Dictionary(value)
    }
}

impl Object {
    pub fn new(value: Value) -> Object {
        unsafe {
            let obj = match value {
                Value::Null => rpc_null_create(),
                Value::Bool(val) => rpc_bool_create(val),
                Value::Uint64(val) => rpc_uint64_create(val),
                Value::Int64(val) => rpc_int64_create(val),
                Value::Double(val) => rpc_double_create(val),
                Value::Date(val) => rpc_date_create(val),
                Value::Fd(val) => rpc_fd_create(val),
                Value::Binary(ref val) => rpc_data_create(val.as_ptr(), val.len(), null()),
                Value::Object(ref val) => rpc_retain(val.value),
                Value::String(ref val) => {
                    let c_val = to_cstr!(val.as_str());
                    rpc_string_create(c_val.as_ptr())
                },
                Value::Array(val) => {
                    let arr = rpc_array_create();
                    for i in val {
                        rpc_array_append_value(arr, Object::new(i).value);
                    }

                    arr
                },
                Value::Dictionary(val) => {
                    let dict = rpc_dictionary_create();
                    for (k, v) in val {
                        let c_key = to_cstr!(k.as_str());
                        rpc_dictionary_set_value(dict, c_key.as_ptr(), Object::new(v).value);
                    }

                    dict
                },
                Value::Error(val) => {
                    rpc_null_create()
                }
            };

            return Object { value: obj };
        }
    }

    pub fn get_raw_type(&self) -> RawType {
        unsafe {
            rpc_get_type(self.value)
        }
    }

    pub fn unpack(&self) -> Value {
        unsafe {
            match self.get_raw_type() {
                RawType::Null => Value::Null,
                RawType::Bool => Value::Bool(rpc_bool_get_value(self.value)),
                RawType::Uint64 => Value::Uint64(rpc_uint64_get_value(self.value)),
                RawType::Int64 => Value::Int64(rpc_int64_get_value(self.value)),
                RawType::Double => Value::Double(rpc_double_get_value(self.value)),
                RawType::String => Value::String(String::from(CStr::from_ptr(
                    rpc_string_get_string_ptr(self.value)).to_str().unwrap())),
                RawType::Date => Value::Date(rpc_date_get_value(self.value)),
                RawType::Binary => Value::Null,
                RawType::Fd => Value::Fd(rpc_fd_get_value(self.value)),
                RawType::Array => Value::Null,
                RawType::Dictionary => Value::Null,
                RawType::Error => Value::Null,
            }
        }
    }
}

impl std::hash::Hash for Object {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {

    }
}

impl fmt::Debug for Object {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        unsafe {
            let descr = rpc_copy_description(self.value);
            let str = CString::from_raw(descr);
            let result = f.write_str(str.to_str().unwrap());
            free(descr as *mut libc::c_void);

            result
        }
    }
}

impl<'a> Call<'a> {
    pub fn result(&self) -> Option<Value> {
        unsafe {
            let result = rpc_call_result(self.value);

            match result.is_null() {
                true => Option::None,
                false => Option::Some(Object { value: result }.unpack())
            }
        }
    }

    pub fn status(&self) -> CallStatus {
        unsafe {
            rpc_call_status(self.value)
        }
    }

    pub fn abort(&mut self) {
        unsafe {
            rpc_call_abort(self.value);
        }
    }

    pub fn resume(&mut self) {

    }

    pub fn wait(&mut self) {
        unsafe {
            rpc_call_wait(self.value);
        }
    }
}

impl Connection {
    pub fn call(&self, name: &str, path: &str, interface: &str, args: &[Value]) -> Call {
        unsafe {
            let c_path = to_cstr!(path);
            let c_interface = to_cstr!(interface);
            let c_name = to_cstr!(name);
            let call = rpc_connection_call(
                self.value, c_path.as_ptr(), c_interface.as_ptr(), c_name.as_ptr(),
                Object::create(args).value, null_block!()
            );

            Call { value: call, connection: self }
        }
    }

    pub fn call_sync(&self, name: &str, path: &str, interface: &str,
                     args: &[Value]) -> Option<Value> {
        let mut c = self.call(name, path, interface, args);
        c.wait();
        c.result()
    }

    pub fn call_async(&self, name: &str, path: &str, interface: &str, args: &[Value],
                      callback: Box<Fn(&Call) -> bool>) {
        unsafe {
            let c_path = to_cstr!(path);
            let c_interface = to_cstr!(interface);
            let c_name = to_cstr!(name);
            let block = ConcreteBlock::new(move |raw_call| {
                let call = Call { connection: self, value: raw_call };
                callback(&call)
            });

            rpc_connection_call(
                self.value, c_path.as_ptr(), c_interface.as_ptr(), c_name.as_ptr(),
                Object::create(args).value, &block
            );
        }
    }
}

impl Client {
    pub fn connect(uri: &str) -> Client {
        unsafe {
            let c_uri = to_cstr!(uri);
            let client = rpc_client_create(c_uri.as_ptr(), null());

            Client {
                value: client,
                connection: Connection { value: rpc_client_get_connection(client)}
            }
        }
    }

    pub fn connection(&self) -> &Connection {
        &self.connection
    }

    pub fn instance(&self, path: &str) -> Instance {
        Instance { connection: &self.connection(), path: String::from(path) }
    }
}


impl<'a> Instance<'a> {
    pub fn interfaces(&self) -> HashMap<String, Interface> {
        self.connection.call_sync(
            "get_interfaces",
            self.path.as_str(),
            "com.twoporeguys.librpc.Introspectable",
            &[][..]
        ).unwrap()
    }

    pub fn interface(&self, name: &str) -> Interface {
        Interface { instance: self, name: String::from(name) }
    }
}


impl Interface {
    pub fn call(method: &str, args: &[&Value]) -> Call {

    }

    pub fn call_sync(method: &str, args: &[&Value]) -> Result<Value> {

    }

    pub fn get(property: &str) -> Result<Value> {

    }

    pub fn set(property: &str, value: &Value) -> Result<()> {

    }
}
