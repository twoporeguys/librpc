The communications architecture of librpc is built around clients and servers, connections and calls. There is a parallel subscribe/notify infrastructure that uses some of the communications components, but that is not the subject of this document.


The Server life-cycle

librpc contexts provides the execution environment for servers and clients. On the server side, a context contains the server rpc_server_t structures for each listening server, and provides a pool of threads that the servers will use to run the response handlers for client requests. Servers require a context to which they will be registered.

A server is created by calling rpc_server_create() with the context and a string specifying the transport and listening address for the server. [NEW] If a server created with that string already exists for the context, an rpc error will be registered and NULL will be returned. Otherwise an rpc_server_t  structure, along with a glib context, event loop, and listening thread will be created. The new server structure will be shared between the calling and listening threads. rpc_server_create() will then wait on a condition variable for notification that the server is ready for connect requests. The listening thread will search for a registered transport that can listen using the uri. If found, the transport's registered listen function will be run.

If the transport is not valid, or if there is an error returned by the transport listen function, an rpc error will be saved in the rpc_server_t structure. The listening thread will signal the condition variable, waking rpc_server_create(). [NEW] If an error has been stored in the server structure, rpc_server_create() will save it for future calls to rpc_get_last_error(), [NEW] call rpc_server_cleanup() to tear down the listening infrastructure it has created, and return NULL.

If the server has been successfully created, the server structure callbacks will be set up support communications between the transport-specific code and librpc. The address of the rpc_server_t structure will be added to the rpc context's array of servers [NEW] under protection of a reader-writer lock. [NEW]. The server will be given a refcnt value of 1 as part of a plan to keep the server structure from being freed when it is closed before its resources have been reclaimed. Finally, the address of the server structure will be returned as a handle that can be used for operations such as requesting that the server broadcast events to its connections.


Server Connections

Requests and replies between clients and servers are implemented using two primary data structure types: connections and calls. Server and client connection state is stored in the 'struct rpc_connection' structure, the allocation of which is represented by the rpc_connection_t type. There are some differences between the use of the structures for client and server operations. Client applications create a client data object which creates a single client-side connection instance. This connection instance is supplied to librpc call APIs. Servers may support multiple connections from different clients.

Server connections are made when the transport listening code receives a connect request. [NEW] If the server is unwilling to accept the connection because it is being closed, it will return (-1) to the transport, which must then tear down the connection and reclaim any resources it has allocated. If the server accepts the connection, it will [NEW] write-lock a reader-writer lock and add the connection to its rs_connections structure, [NEW] bump its rs_refcnt count to prevent itself from closing while a connection may still be communicating with it, and [NEW] call the connection's reference counting function to increase the connection refcnt to prevent the connection from going away while the server still has its handle stored in rs_connections.

Applications and other parts of librpc may call server functions to respond to calls or broadcast messages to all of its connections. [NEW] If the server is closing it will return an error in the call response case and ignore broadcast requests. [NEW] When broadcasting, the server will walk through its rs_connections using read-lock protection.

Transports are required to register an abort function in the rpc connection structure. However it is implemented in the transport, this function must block until the connection has been torn down and the rpc connection rco_close handler has been called. After this point the transport must reference neither the rpc connection structure nor any of its own resources related to the connection as they will have been reclaimed.

Connections may be torn down either as a result of an error or by application action. If a client closes an application, it will be detected by the transport and communicated to the server when the transport calls the rco_closed handler for the server connection and the rpc code calls [NEW] rpc_server_disconnect() for the connection. If the server is not shutting down, rpc_server_disconnect will remove the connection from rs_connections under writer-lock protection, and cause the server's reference to the connection to be dropped. The connection shutdown code [NEW]  will then call rpc_server_release() to drop its reference on the server. After the references are dropped, neither component will access the released structures.

When a server is being closed via rpc_server_close(), there may be interactions with connections that are connecting, running, or independently aborting or closing. Also, different transports have differing mechanisms for shutting down. Interactions with the glib main loop must also be considered to avoid poll errors: for sockets, cancelling any outstanding posted asynchronous accepts is a requirement, whereas websockets requires that the main loop be exited before doing the disconnect.

A transport must register a teardown function before its rpc server may be closed. This must disallow any new connections from being made. Some transports use the listening thread for delivering incoming data and protocol messages as well. They must set the server structure  boolean rs_threaded_teardown to true in their listen functions so that the thread will continue to run until all existing connections are closed.

A closing server will set the boolean rs_closed to true. That will cause any calls to accept new connections or dispatch calls to return an error, and calls to disconnect or broadcast to silently fail. The server will call rpc_close_connection() for each connection stll remaining in its rs_connections structure. If the connection is still running, it will be aborted. The transports are required to make the call to their registered abort handlers a blocking call; by the time the call returns to the server, the connection will have released its reference on the server (by calling rpc_server_reease). rpc_connection_close() will return an error (-1) to the server which indicates to the server that it must drop the reference it holds on the connection because, having marked itself closed before making the function call, the call to rpc_server_disconnect will not have dropped the reference.

If the connection has already aborted because the client has closed, the call to rpc_connection_close() will not call the transport abort code. Since the connection will have already released its references to both the server and itself, rpc_connection_close() will drop the final reference to the connection, and return 0/success to the server, indicating that the server no longer should access the connection.

When rpc_connection_reference_change() is called for a connection has only one reference, the transport's registered release function will be called and all connection resources will be reclaimed.

Once the server has closed all connections and has only a single reference to itself it will be freed.


------------to be continued------

Server Pause/Resume

The functions [NEW]  rpc_server_resume() and rpc_server_pause() control whether incoming client request calls are delivered to listening applications or queued. Currently the servers are created in a paused state and the code that calls rpc_server_create() must call rpc_server_resume when ready to allow requests to be delivered. [TODO, this could be a configuration setting delivered in the call to rpc_server_create().] If a server is paused,


Client Connections
Calls
....


