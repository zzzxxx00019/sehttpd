# seHTTPd

`seHTTPd` implements a small and efficient web server with 1K lines of C code.
I/O multiplexing is achieved using [io_uring](https://kernel.dk/io_uring.pdf?source=techstories.org).

## Features

* Single-threaded, non-blocking I/O based on event-driven model
* HTTP persistent connection (HTTP Keep-Alive)
* A timer for executing the handler after having waited the specified time

## High-level Design

```text
+----------------------------------------------+
|                                              |
|  +-----------+   wait   +-----------------+  |  copy   +---------+
|  |           +---------->                 +------------>         |
|  | IO Device |    1     | Kernel's buffer |  |   2     | Process |
|  |           <----------+                 <------------+         |
|  +-----------+          +-----------------+  |         +---------+
|                                              |
+----------------------------------------------+
```

## Build from Source

At the moment, `seHTTPd` supports Linux based systems with epoll system call.
Building `seHTTPd` is straightforward.
```shell
$ make
```

By default the server accepts connections on port 8081, if you want to assign
other port for the server, modify file `src/mainloop.c` and build again.

## License
`seHTTPd` is released under the MIT License. Use of this source code is governed
by a MIT License that can be found in the LICENSE file.
