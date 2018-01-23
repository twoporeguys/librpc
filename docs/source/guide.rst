User's guide
============

Introduction
------------
librpc is a general purpose IPC and RPC library. It supports creating clients
and servers, sending asynchronous notifications, data streaming and file
descriptor exchange. Interface is loosely based on Apple XPC.

librpc endpoints exchange structured data. Data structure is based on a number
of primitive data types that can form structure similar to JSON or YAML.

Data types
----------
Following primitive data types are defined:

- ``null``
- ``bool``
- ``int64``
- ``uint64``
- ``double``
- ``string``
- ``date``
- ``binary``
- ``fd``
- ``array``
- ``dictionary``
- ``error``
- ``shmem`` (supported only on Linux)

Arrays are lists of values constructed of any librpc types. Dictionaries are
maps of string keys to values of arbitrary type.

Requests, responses and events
------------------------------
The most basic form of librpc communication are requests. A client can send
a request to the server and get back a response, an error or a streaming
response.

Both servers and clients can also send `events` to each other. Events are
messages containing event name and event payload and are not acknowledged
by the receiving end.

Instances, interfaces and functions
-----------------------------------
Servers expose a number of `functions` to the clients. Functions are grouped
in namespaces using dotted syntax: ``namespace1.namespace2.function``.

Transports
----------
Clients can talk to servers using multiple transports. A transport is an
implementation of underlying protocol passing librpc messages around.
Currently, librpc includes following transports:

- ``tcp`` - TCP/IP sockets
- ``ws`` - WebSockets
- ``unix`` - Unix domain sockets
- ``usb`` - USB transfers (used to talk to embedded devices implementing librpc
  servers over USB)

Endpoint addresses
------------------
An address of librpc server (or more generally speaking, librpc endpoint) is
encoded as a URI, with scheme field denoting requested transport.

Endpoint address examples:

- ``tcp://192.168.0.1:5000``
- ``ws://server.local/path``
- ``unix:///var/run/server.sock``
- ``usb://Device#123`` (``Device#123`` part of the example is a USB device
  serial number)

Object model
------------

Client operation
----------------

Server operation
----------------

Using Blocks
------------
librpc heavily relies on a C language feature called "Blocks". Blocks support
is present by default in the clang compiler and patches adding same
functionality exist for gcc. For more information, please refer to
`Working with blocks <https://developer.apple.com/library/content/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/WorkingwithBlocks/WorkingwithBlocks.html>`_
article.