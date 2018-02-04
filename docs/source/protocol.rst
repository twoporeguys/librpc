Protocol specification
======================
librpc messages are dictionary objects serialized using MessagePack format.
There are several extension types defined for data types not available
in the MessagePack specification.

For the sake of readability, examples shown below use JSON format that
represents the desired structure of the message.

Extension types
---------------

+----------------+-----------------------+-------------------------------------+
| Exttype number | Name                  | Encoding                            |
+================+=======================+=====================================+
| 1              | Date                  | 64-bit signed integer               |
+----------------+-----------------------+-------------------------------------+
| 2              | File descriptor       | 32-bit unsigned integer             |
+----------------+-----------------------+-------------------------------------+
| 3              | Shared memory segment | 32-bit unsigned integer (fd number) |
+----------------+-----------------------+-------------------------------------+
| 4              | Error                 | Nested MessagePack dictionary       |
+----------------+-----------------------+-------------------------------------+

Error format
~~~~~~~~~~~~
Error dictionaries consist of the following fields:

- ``code`` - integer errno code
- ``message`` - error description
- ``stacktrace`` - server stack trace (optional)
- ``extra`` - additional error data (optional)


Request message
---------------
Sent by a client to a server

Normal response message
-----------------------
Sent by a server to a client

Error response message
----------------------
Sent by a server to a client, in case of error

Streaming response fragment message
-----------------------------------
Sent by a server to a client, indicating that more responses will follow.

Streaming response end message
------------------------------

Request abort message
---------------------

Event message
-------------

Request ID generation
---------------------
Request ID can be any string unique on the server for the duration of the
request. However, by convention, request IDs are UUID version 4 strings.