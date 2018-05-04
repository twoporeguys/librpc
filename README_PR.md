## PR Changes, "resume_and_attributes"  

This branch started out addressing changes to make pause and resume work, and to get the null path and interface code to be more robust. At the time I was testing with the example code server.c, client.c, and example.c code and wanted to get them all to work as well as support multiple transports, clean up all resources used, and report server and client failures to the listening threads. The PR outgrew its name in the process...  

### Files changed  
##### examples/client/client.c  
- used for testing multiple transports. Commented transports were left in.  

##### examples/server/server.c  
- ditto. Plus added rpc_server_resume() and closing the server after 30 seconds.  

##### example/event/event.c  
- Relaced the bogus null format with RPC_NULL_FORMAT, defined as "[]". Also checked for a null client.  

##### include/rpc/connection.h  
- Added RPC_NULL FORMAT. Fixed Nullable and Nonnull attributes.   
- Added definitions for rpc_server_find() and rpc_server_pause.  

##### include/rpc/service.h  
- Added a few comments to definition headers.   
- Fixed Nullable and Nonnull attributes.   

##### src/internal.h  
- Added typedef definitions for new callbacks in the rpc_connection, rpc_server and rpc_context structures.   
- Added ric_strings to struct_inbound_call as a dictionary to hold strings for paused calls after their frame has been released.  
-  Replaced mutexes with reader-writer locks where appropriate and added new locks.   
- Added rco_icall_rwlock and rco_call_rwlock to struct rpc_connection_t for protecting incoming and outgoing calls when adding, removing, and doing lookups.   
- Added rco_error to rpc_connection_t for storing rpc errors across threads.  
- Added rco_aborted to rpc_connection_t to indicate that rpc_close() (the connection rco_close function) has been called. It is called when either of the socket_reader threads goes away, and when the websockets "close"signal is emited for client and server side connections.  
- Added rco_refcnt to rpc_connection_t. It is increased  whenever a call is made that might otherwise result in the rpc connection and the transport-specific connection data accessed via rco_arg being freed when the caller still may need to reference them. It is decreased when the caller no longer needs access. Callers that hold a connection handle manipulate rco_refcnt by calling rpc_connection_reference_change().  
- Added rco_server_released to indicate that a server-side connection has released interest in its server. This is used when determining the completion status of connection cleanup.  
- Added function prototypes for rpc_server_disconnect(), rpc_server_release(), and rpc_server_quit(), called (respectively) when a connection is removing itself from the server's interest, when a connection is giving up its interest in the server, and when a transport needs to terminate its thread.  

###### To struct rpc_server:  
- Added callbacks allowing connections to determine whether the server is open, diconnect themselves from the server, and finalize teardown if the transport requires extra steps with running threads for cleaning itself up (Websockets needs this for example).  
- Added rs_error to allow failures in server threads to be conveyed aross thread boundaries.  
- Added flags: rs_closed to indicate that the server is closing; rs_threaded_teardown for the transport to convey its requirement to the server  
- Added the rs_calls queue and protecting mutex rs_call_mutex for storing incoming calls received while a server is paused.  
- Added some counters for tracking counts of connections made, aborted etc.   

###### To struct rpc_context:  
- Added a reader-writer lock to protect access to the context server array.  

##### src/rpc_connection.c   
- Added static function rpc_connection_free_resources() to be called when a connection refcnt has fallen to 1 (representing the connection's self-reference) and resources can be freed.  
- Added static function rpc_connection_abort() which only sets the aborted flag. This is a default in case a transport doesn't provide one.  
- Added function rpc_connection_reference_change(). It is called to either retain or release a reference to the connection. It will free a connection and its resources if called with a refcnt of 1.  
- Added static function rpc_connection_release_call() to free inbound call resources.  
- Added locking around the hash tables for incoming and outgoing calls.  

###### rpc_connection_alloc():  
- initializes the connection refcnt.  
- Now sets rco_arg to conn just like rpc_connection_create() so that its resources can be recovered.  
- sets the connection rco_abort function pointer to the new default function.  

###### rpc_connection_create:  
- initializes the new rco_call_mutex needed for server pause and resume call queueing.  
- sets the refcnt and rco_abort as discussed for the server side.  

###### on_rpc_call():  
- checks the value returned from the call to rpc_server_dispatch. If it fails because the server is closing it calls rpc_function_error() and rpc_connection_close_inbound_call() to notify the caller and reclaim the inbound call resources.  
- Whenever a call timeount source is destroyed the rc_timeout is set to NULL  
- If rpc_call_free() is called on a call that still has an active timeout the timeout is destroyed.  
- on_rpc_abort does not remove the aborted call from the connection's inbound call table; that is handled when the call is closed after the receiver wakes up and is able to get call status and then call rpc_close_inbound_call().  
- Handlers are set to null after any Block_release calls. Existing Handlers are released prior to being released with new ones.  

###### rpc_close():  
- The connection rco_aborted flag indicates this has been run. It is checked to ensure it is only run once.  
- rco_closed is not set universally set. It is reserved to indicate that the connection has been explicitely closed by a caller (in the client case) or when a server connection has no pending inbound calls and rpc_connection_close() can be immediately called. Note that rco_closed will also be set for a server connection when rpc_close_inbound_calls is closing the connections final call prior to calling rpc_connection_close().  
- If there are no calls pended on a server connection's inbound call table, rco_closed will be set to true and rpc_connection_call will be called immediately.  
- If there are no calls pended on a client connection's call table and rco_closed is already true (indicating that rpc_connection_close has been called prior to the connection being aborted) rpc_connection_close will be called to drop the connection's self reference and potentially free it.  
- Reader-writer locks in read lock mode  are used to protect the call queues. As before, all calls on the queues have their condition variable signalled to wake blocked reply threads.  

###### rpc_close_inbound_call:  
- If the call is the last call in the rco_inbound_calls hash table and rco_closed is set, rpc_connection_close() is called to finalize connection cleanup.  

###### rpc_connection_alloc:   
- Initialize new flags and callbacks   

###### rpc_connection_create:  
- Initialize new flags and callbacks.  
- If the connect attempt fails, set the flags correctly and use rpc_connection_close to reclaim resources.  

###### rpc_connection_close:  

Changed to handle reference counting and difference between server and client connections.  

###### On the client side:  
- The function can be called either directly by the client, or once a client has called this function and caused an abort to be issued, after the abort has been handled and all calls waiters have been awakened (either from rpc_call_free or rpc_close).  
- If the connection is considered active (!conn->rco_aborted), rco_closed will be set to show that a client has initiated the action. The connection reference count will be raised and lowered around the call to the transport abort function registered in conn->rco_abort. This insures that the connection still exists throughout the abort process and call to rpc_close().  
- If the connection still has open calls, the caller will exit, counting on rpc_call_free to call rpc_connection_close once all calls have been completed.  
- Otherwise, rpc_connection_reference_change will be called before the function exits, returning 0. If there is only 1 reference on the connection, the registered transport rco_release function will be called and all connection resources will be reclaimed.  

###### On the server side:  
- If the connection has not been aborted, the transport abort function will be called, and -1 will be returned to indicate to the caller that the connection had not previously been aborted/closed (and that the server needs to explicitely drop its reference to the connection. Otherwise, rpc_connection_close has been or will be called either directly from rpc_close or rpc_close_inbound_connection() after the connection's last call has been handled. rpc_server_disconnect will be called indicating that the server should give up its interest in the  connection. If the server is not closing, it will drop its reference on the connection and remove the connection from its rs_connections structure. Otherwise it will silently fail, deferring that to its own cleanup.  
- rpc_server_release() will be called to drop the connection reference to the server  
- rpc_connection_reference_change() will be called to decrement the connection reference count and possible free all related resources.  

###### rpc_connection_reference_change:  
- if called with retain==true, increases the connection ref count. Otherwise, decrements it if the count is greater than 1, or frees connection resources if it equals 1.  

####### rpc_connection_free:  
- Currently does nothing and should be removed.  

###### rpc_connection_is_open:  
- Returns true if the connection is not closed or aborted.  

###### rpc_connection_call:  
- Calls rpc_connection_is_open() to detemine whether a call may be made. If not, calls rpc_set_last_error() and returns NULL.  

##### src/rpc_server.c  

- Added static function rpc_server_cleanup to stop the listen thread if needed and clean up glib resources alocated for the server  
- Added function rpc_server_quit to allow a transport to control destruction of the server listening thread.  
- Added rpc_server_valid as a check that the server sn't shutting down.  
- Added static function server_queue_purge is support of pausing (or bringing up in a paused condition)the server dispatch of calls; while a server is paused, all incoming calls are held on a queue. When the server is resumed, the calls are dispatched to their target receivers, or rejected with errors if the server is closing.  
 
###### rpc_server_accept:  
- Returns an error if the server is closing (and bumps a counter rs_conn_refused)  
- Adds the new connection to the servers rs_connections under reader-writer write lock protection (and bumps a counter for connections accepted)  

###### rpc_server_disconnect:  
- New. Silently returns if the server is closing, otherwise, removes the connection from rs_connections and drops the reference added when the connection was accepted.  

###### rpc_server_listen:  
- Sets the server rs_error field with any errors encountered   
- Looks for errors returned by the transport and only sets rs_operational when the listen is successful.  

###### rpc_server_find:  
- New. Searches the context for a server created with the specified URI. Returns the server handle if found, otherwise, NULL.  

###### rpc_server_create:  
- Checks for an existing server on the context   
- Initializes new fields in the server structure.    
- Checks the server structure for a listen error and saves it.   
- If the server listen fails, calls rpc_server_cleanup() to stop the glib mainloop and reclaim the resources.  

###### rpc_server_broadcast_event:  
- Protects the server rs_connections structure with a reader lock.  
- Silently fails if the server is marked closed.  

###### rpc_server_dispatch:  
- Changes to support server pause/resume  
- Uses the new rs_calls_mtx mutex to protect the new server calls queued. If a server is paused, or if the queue is not empty, adds the call to the queue and returns success. A dictionary is added to the inbound call structure to preserve any strings currently in the call.  
- If the server is closed, returns an error.  
- If the server is not closed and the queue is empty, calls rpc_context_dispatch()  for the call.  

###### server_queue_dispatch:  
- Does not take hold locks. If required, that is the caller's responsibility.  
- Takes calls off of the queue in the order they were received, retreves strings stored in the call's ric_strings dictionary, and either dispatches them or drops them. If the server is closing or rpc_context_dispatch() returns an error, rpc_close_inbound_call is called to reclaim the resources.   

###### rpc_server_resume:  
- If the server is not closing, calls server_queue_purge to dispatch queued calls under protection of the rs_calls_mtx mutex.  

###### rpc_server_pause:  
- New. If the server is not closing, sets rs_paused to true under protection of the rs_calls_mtx mutex.  

###### rpc_server_release:  
- New. Removes a reference to the server under protection of the server rs_mtx mutex. If the server ref count is 1, uUnder protection of the context reader-writer lock, removes the server from the context servers array.  

###### rpc_server_close:  
- Returns an error if the transport has no teardown function registered.  
- Under protection of the rs_mtx, sets the server rs_closed boolean to true and purges any queued calls.  
- If the transport has not set threaded_server to true, calls rpc_server_cleanup to stop the server listen thread and reclaim related glib resources.  
- Calls the transport's teardown function.  
- If the server connection array is not empty, closes all connections in the array. If rpc_connection_close() returns an error, dereferences the connection (since rpc_server_disconnect will silently fail to do so).    
- If the server equires a threaded teardown, calls the registered teardown_end function and follows that with the delayed call to rpc_server_cleanup.  

##### src/rpc_client.c:  
- Added code in rpc_client_close to keep the connection from going away until fully cleaned up. This is especially important with websockets, which requires the client mainloop to be running in order for it to finish closing, reclaim resources, and report back any errors.  

##### src/rpc_service.c:  
Various changes to support null interfaces and paths, and inbound call error and abort handling and resource reclamation;  In particular, anywhere a hash table lookup is done, care has been taken to insure that the key is non-null (required because the key type is specified as a C-string).  
Other changes include:  
- protection of call and inbound call tables with reader-writer locks.  
- rpc_context_emit_event() walks the context server array holding a reader lock.  
- more attention to whether calls have had resonses or been aborted when handling their disposition.  
- resources are reclaimed in rpc_instance_unregister_member().  
- rpc_function_respond() check if the call has already responded before responding.  

##### src/transport/loopback:  
- In loopback_listen, properly handle malformed URIs. Make sure that an invalid URI doesn't look like a request to listen on channel 0.  

##### src/transport/socket:  
Added code to allow teardown, error reporting and UNIX domain sockets to work.  

###### socket_parse_uri:  
- Don't crash on a bad URI.  
- soup parsing for UNIX domain sockets has issues, especially for relative paths. Handle this.  

###### socket_accept:  
- Interactions with glib/gio are rearranged to allow better cleanup in the event of errors.  
- On failure from g_socket finish, only reschedule the next accept if the server is still viable.  
- Properly cleanup resources if the server rejects a call to rpc_server_accept (srv->rs_accept)  
- Reset the glib cancellable object and use it when scheduling the next accept; track that an accept is outstanding.  
- Set up rco_release for server-side connections.  
			
###### socket_connect:  
- Changes to improve cleanup if connect() fails.  

###### socket_listen:  
- Store rpc_error objects in the rpc_server structure for various failures.  
- Separate acquiring the UNIX domain socket address from the GSocketaddr from using it to determine if a file already exists; this eliminates a glib assert in the test framework.  
- Create a glib cancellable object and store it in the socket_server structure. This is required in order to cancell an outstanding accept when stopping server istening. Use this object in calls to g_socket_listener_accept_async().  

###### socket_abort:  
- Use an mutex and flag (conn->sc_aborted) in the socket_connection structure to prevent socket_abort from being called multiple times. This scenario is possible if rpc_connection_close is called after an abort has been started but before rpc_close() has run.  

###### socket_release:  
- Check for the existance of resources individually before cleaning them up to handle cleanup of a partially initialized socket_connection structure.  
 
###### socket_teardown:  
- If there is an outstanding asynchronous accept request pending, cancel it before issuing the g_socket_listener_close() call.  

##### src/transport/ws.c:  
Added code to allow websockets to be safely and completely closed. In the current implementation the listener thread is also used for receiving messages and libsoup signals. If a websocket connection is closing/aborting, librpc will not know until the "closed" signal is received. This has some impacts:  
1) When a client is closing, librpc will not be able to recover resources if the g_main_loop exits before the "close" signal is emitted.   
2) When a websocket is being closed/aborted the abort code needs to stay blocked until the "close" has been handled.  
3) Server teardown must be done in two parts because websockets needs the listening thread until all connections have been closed.. New connections first need to be prevented. Then after the server closes (aborts) any open connections, a second teardown routine (rco_teardown_end) must be called to disconnect the soup_server and clean up any remaining resources. After this the rpc_server can finish cleaning up and may release its own resources.  
4) The ws_connection and ws_connection structures have been extended with new fields as needed.  

###### ws_listen:  
- Now reports listen error back to the server_create thread by saving them in the rpc server structure.   

###### done_waiting:  
- New. Runs in the listen thread to process a glib source added as part of the final teardown code. It must exit the listen thread before soup_server_disconnect() closes any sockets it manages to prevent glib poll errors.  

###### ws_teardown:  
- New. Initiates teardown by removing libsoup handlers for future connection attempts.  

###### ws_teardown_end:  
- New. Works with done_waiting() to flush the glib main loop and then disconnects the soup server before freeing ws_server resources.  

###### ws_process_connection:  
- Checks the results of the call to rs_accept() cleans up resources on failure.  
- Sets up ws_release for the server-side connection so that those resources can be released if the connection closes.  

###### ws_close:  
- This is called from libsoup when either any connection has been aborted or closed (because its peer closed). It sets wc_aborted because ws_abort()  might not have been called, calls the rco_close() function, and then signals the ws_abort_cv condition variable in case a call to ws_abort is blocked on it.  

###### ws_abort:  
- If the connection has not already been aborted it asks libsoup to lose the connection and then waits on a condition variable for a signal from ws_close().  

###### ws_release:  
- Only frees or derefs objects that were initialized. It is set up to free resources for server and client connections.  

##### src/transport/xpc.c:  
- xpc_listen()  saves any errors in the rpc server structure to be reported in the right thread.   

##### tests/api/server.c:  
- New. Populated the framework with a variety of tests.  

 


