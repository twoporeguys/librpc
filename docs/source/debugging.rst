Debugging
=========

Message tracing
===============
librpc can log all sent and received messages to standard error stream or to
a file. In order to enable that feature, set ``LIBRPC_LOGGING`` variable to
either ``stderr`` string or to a path, where message trace should be written.
