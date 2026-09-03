# Communo

Communo project is a client and server applications for educational purposes. Both can communicate and exchange informations based on TCP/IP protocols and TLS support. There is no fixed goal for this project.

# Features

* Supported packet viewing / logging in debug mode
* Logging system for the server (syslog, console, ...)
* TLS implementation using self-signed certificate toolchain
* Interactive mode with a CLI for the client
* Linux support for the server
* Windows and Linux support for the client
* Localhost binding for the client and server

# Requirements

* cmake >= 3.14
* C++ compiler with C++17 support
* OpenSSL >= 3.5.x

# How to compile

Use cmake and specify the target (server/client) you want to use. Default is all.
cmake will generate self-signed certificate chain (CA, Cert, Key), with their path/location passed to the executables.

## Server

```Bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Client

Same for the client on Linux, IDE-dependent on Windows.

# How to use

## Server

```Bash
$ ./Server --help
Options:
  --no-daemon
  --debug
  --help
```

By default, the server will run in the background unless the `--no-daemon` option is specified.
The `--debug` option will trigger extra logging information (packet management, ...)

## Client

Launch the executable and see what happens / what options are available. The server must be running for the client to work.

The current feature involves: echoing a message sent from te client using the CLI + the `> echo <xxx>` command.
