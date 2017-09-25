Protocol specification
======================
librpc messages are dictionary objects serialized using MessagePack format.


Request message
---------------
Sent by a client to a server


Normal response message
-----------------------


Error response message
----------------------


Streaming response fragment message
-----------------------------------

Request ID generation
---------------------
Request ID can be any string unique on the server for the duration of the
request. However, by convention, request IDs are UUID version 4 strings.