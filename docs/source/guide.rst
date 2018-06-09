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

Object handles
--------------
librpc provides an universal data type that can store any kind of data
supported by the library. When created, librpc returns an opaque handle,
(called object handle) to the value. The type of the value referenced by the
object handle is immutable. Actual value is also immutable for all data types
except ``array`` and ``dictionary``. librpc owns the value referenced by the
object handle - no externally managed data is referenced (with ``binary``
type being an exception, see :ref:`binary-data-handling`.

librpc objects are reference counted. Each newly created object has a
reference count of 1. Reference count can be increased using ``rpc_retain()``
function or decreased using ``rpc_release()`` function. When reference count
of an object drops to 0, object handle and the associated value is released.

A number of functions exist to operate on object handles - that involves
creating a new objects, retrieving values from the objects, reference count
management, copying objects and checking for their equality. There's too many
of them to cover each and every one here - please see API reference for
``rpc/object.h`` file for details.

Binary data handling
--------------------
.. _binary-data-handling:

``binary`` data type is kind of special - it's the only type that can
reference an external data buffer. This behavior has been designed to avoid
possibly expensive process of copying large data buffers. In order to allow
the buffer to be finally released, ``rpc_data_create()`` function allows
the user to pass a destructor block to be called when reference count of the
object drops to 0. Destructor block, if not ``NULL``, should be passed
directly from the stack because it's automatically copied to the heap and
released by the library.

Requests, responses and events
------------------------------
The most basic form of librpc communication are requests. A client can send
a request to the server and get back a response, an error or a streaming
response.

Both servers and clients can also send `events` to each other. Events are
messages containing event name and event payload and are not acknowledged
by the receiving end.

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
See :ref:`object-model`

Client operation
----------------

Client and connection handles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Client operation starts by obtaining a `client handle` using
``rpc_client_create()`` function. This handle, however, is quite useless
without a `connection handle` that is actually used to interact with the
server. Said handle can be obtained using ``rpc_client_get_connection()``
function.

Sending calls to the server
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The most ordinary way to interact with a librpc server is to send a method
call request. A number of functions exist for that purpose - the most simple
to use one is ``rpc_function_call_syncp()``:

.. code-block:: c

   /* An example of sending a call to the server and getting result */
   rpc_client_t client = rpc_client_create("ws://127.0.0.1:8080/server", NULL);
   rpc_connection_t conn = rpc_client_get_connection();
   rpc_object_t result = rpc_connection_call_syncp(conn, NULL, NULL, "hello",
       "hello", "[s]", "world");

   /* Print the result of the call */
   printf("%s\n", rpc_string_get_string_ptr(result));

Server operation
----------------

Registering objects
~~~~~~~~~~~~~~~~~~~

Using Blocks
------------
librpc heavily relies on a C language feature called "Blocks". Blocks support
is present by default in the clang compiler and patches adding same
functionality exist for gcc. For more information, please refer to
`Working with blocks <https://developer.apple.com/library/content/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/WorkingwithBlocks/WorkingwithBlocks.html>`_
article.

FAQ
---

I get an error like ``ModuleNotFoundError: No module named 'librpc'``.
How do I fix it?

Run ``brew reinstall librpc --with-python``.
Likely cause is that a ``brew upgrade`` command reinstalled librpc without the
python bindings. 
