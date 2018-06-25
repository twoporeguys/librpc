Building
========

Setting up development environment
----------------------------------
librpc uses the `CMake <http://www.cmake.org>`_ build system. Currently it
runs on Linux, FreeBSD and macOS systems.

There is a makefile that wraps most of the typical CMake usage scenarios,
though. Running ``make bootstrap`` from the top level directory of the source
checkout should be enough to set up a development environment.

Compile-time configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~
Following compile-time parameters can be passed to CMake:

+==============
| Name
+----------------------------+---------+
|``RPC_DEBUG``               | ``OFF`` |
+----------------------------+---------+
| ``BUNDLED_BLOCKS_RUNTIME`` | ``OFF`` | Use bundled version of
|                            |         | libBlocksRuntime         |
+----------------------------+---------+---------------------------
| ``BUILD_DOC``              | ``OFF`` | Build documentation
+----------------------------+---------+
| ``BUILD_TESTS``            | ``ON``  | Build test suite
+----------------------------+---------+
| ``BUILD_EXAMPLES``         | ``ON``  | Build example programs
+----------------------------+---------+
| ``BUILD_PYTHON`` (``ON``/``OFF``)
+----------------------------+---------+
| ``PYTHON_VERSION`` (string)
+----------------------------+---------+
| ``BUILD_CPLUSPLUS`` (``ON``/``OFF``)
+----------------------------+---------+
| ``BUILD_JSON`` (``ON``/``OFF``)
+----------------------------+---------+
| ``BUILD_LIBUSB`` (``ON``/``OFF``)
+----------------------------+---------+
| ``BUILD_RPCTOOL`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_UBSAN`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_UBSAN_NULLABILITY`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_ASAN`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_LIBDISPATCH`` (``ON``/``OFF``)
+----------------------------+---------+


Remote debugging with VS Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
These instructions were adapted from https://medium.com/@spe_/debugging-c-c-programs-remotely-using-visual-studio-code-and-gdbserver-559d3434fb78

Here is the situation. I have a remote Ubuntu host that can run the `rpcd` tool and I'm testing connecting rpc services to this host over tcp on my mac. \
I want to debug the `rpcd` service as these connections are happening and be able to edit and make changes to the rpcd code. Follow the instructions in the link.
Once you have `gdbserver` and `gdb` running correctly on both machines and with the remote directory containing the `librpc` repo mounted (Let's say `/home/rpc_user/Git/librpc`) to
a local directory (`/Users/brett/Git/librpc_remote`). First create a script we'll use to launch `rpcd` by calling build with the appropriate flags. We'll also use this command to create an ssh tunnel to the gdbserver port.
This should go in the remotes librpc root directory:
```
# Kill gdbserver if it's running
ssh rpc_user@<REMOTE_HOST> killall gdbserver &> /dev/null
# Compile myprogram and launch gdbserver, listening on port 9091
ssh \
  -L9091:localhost:9091 \
  rpc_user@<REMOTE_HOST> \
  "cd ~/Git/librpc && make RPC_DEBUG=ON BUILD_TYPE=Debug && gdbserver :9091 ./build/tools/rpcd/rpcd -l tcp://0.0.0.0:5002"
```

Now on your local machine, open the local folder in VS Code and create a new Debug Configuration defined as follows:
```
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C++ Launch",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceRoot}/build/tools/rpcd/rpcd",
            "miDebuggerServerAddress": "localhost:9091",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceRoot}",
            "environment": [],
            "externalConsole": true,
            "sourceFileMap": {"/home/rpc_user/Git/librpc": "/Users/brett/Git/librpc_remote"},
            "preLaunchTask": "prepare_remote_debug",
            "linux": {
              "MIMode": "gdb"
            },
            "osx": {
              "MIMode": "gdb"
            },
            "windows": {
              "MIMode": "gdb"
            }
        }
    ]
}
```
You'll need to set the arguments of `"sourceFileMap"` correctly to have the debugger be able to find the source files the mounted folder. Now we need to create \
the `prepare_remote_debug` task that will launch our shell script we made above. Create a new VS Code task or create a `tasks.json` file in the the `.vscode` folder with the following:
```
{
    // See https://go.microsoft.com/fwlink/?LinkId=733558
    // for the documentation about the tasks.json format
    "version": "2.0.0",
    "tasks": [
        {
            "label": "prepare_remote_debug",
            "type": "shell",
            "command": "./prepare_remote_debug.sh",
            "args": [],
            "isBackground": true,
            "presentation": {
                // Reveal the output only if unrecognized errors occur.
                "reveal": "silent"
            },
            "problemMatcher": {
                "owner": "custom",
                "pattern": [
                    {
                        "regexp": "\\b\\B",
                        "file": 1,
                        "location": 2,
                        "message": 3
                    }
                ],
                "background": {
                    "beginsPattern": ".*mkdir -p build.*",
                    "endsPattern": ".*Listening on port.*"
                }
            }
        }
    ]
}
```
We use some VS Code trickery here to enable this task to continue to run while still flagging the debug task that it is finished (look at the `problemMatcher` entry).
Now when you run this debug task it should auto-build and launch the `rpcd` service and attach the debugger properly. Have fun!
