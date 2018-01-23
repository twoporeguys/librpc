Protocol specification
======================
librpc messages are dictionary objects serialized using MessagePack format.
There are several extension types defined for data types not available
in the MessagePack specification.

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
* ``code`` - integer errno code
* ``message`` - error description
* ``stacktrace`` - server stack trace (optional)
* ``extra`` - additional error data (optional)


Request message
---------------
Sent by a client to a server


Normal response message
-----------------------


Error response message
----------------------


Streaming response fragment message
-----------------------------------

Event message
-------------

Request ID generation
---------------------
Request ID can be any string unique on the server for the duration of the
request. However, by convention, request IDs are UUID version 4 strings.