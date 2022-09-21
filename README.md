![](./misc/banner.png)
=====

Coproto is a flexible *C++14* or *C++20* cross-platform protocol framework based on coroutines. The primary design goal of Coproto is to enable easy-to-write, high-performance protocols that can be run in any environment. Protocols are written in a synchronous manner (see below) but can be executed asynchronously across one or more threads. See the tutorials [C++20](https://github.com/Visa-Research/coproto/blob/master/frontend/cpp20Tutorial.cpp), [C++14](https://github.com/Visa-Research/coproto/blob/master/frontend/cpp14Tutorial.cpp), [Custom Socket](https://github.com/Visa-Research/coproto/blob/master/frontend/SocketTutorial.cpp).

### Features:
* **Coroutine abstraction**: Protocols can be written in a synchronous manner and evaluated in an asynchronous manner.
* **Backwards compatibility**: Coproto uses the C\++20 coroutine model but still allows for code to run on C++14.
* **Concurrent composition of multiple protocols**: Multiple protocols can be concurrently executed on a single socket. Coproto ensures that each concurrent protocol receives the correct messages. 
* **Single or multi-threaded**: A protocol can be executed on multiply threads while sharing a single socket. Coproto manages the logic required to ensure each thread/sub-protocol gets the correct messages.
* **Local or network communication**: Coproto does not mandate any particular socket type, e.g. *posix, boost::asio*, but instead allows the user to integrate their socket of choice. ALternatively, the included BufferingSocket allows the caller to get/set the next message for any protocol. 
* **Boost Asio and OpenSSL**: The library can be built with Boost Asio TCP and OpenSSL TLS support.
* **Test with network error injection**: Test the robustness of the protocol by injecting networking errors or by modifying protocol messages.
 


**C++20 Echo server example:** 
With C++20 the coroutine machinery can be used. Each socket operations can be `co_awaited` which [possibly] pauses the current protocol and allows other work to be performed by the current thread, e.g. concurrently execute some other protocol. See the [C++20 tutorial](https://github.com/Visa-Research/coproto/blob/master/frontend/cpp20Tutorial.cpp).
```cpp
task<> echoClient(std::string message, Socket& socket) {
    co_await socket.send(message);
    co_await socket.recv(message);
}
task<> echoServer(Socket& socket) {
    std::string message;
    co_await socket.recvResize(message);
    co_await socket.send(message);
}
void echoExample()
{
    auto sockets = LocalAsyncSocket:makePair();
    
    // lazily construct protocol objects
    auto server = echoServer(sockets[0]);
    auto client = echoClient("hello world", sockets[1]);

    // Execute both protocols locally on the current thread.
    sync_wait(when_all_ready(std::move(server), std::move(client)));
}
```

**C++14 Echo server example:**
To achieve the same functionality in C++14, the library resorts to macros to emulate coroutines.  Each protocol begins with the `MC_BEGIN` macro which performs a [lambda capture ](https://en.cppreference.com/w/cpp/language/lambda) of all local variables to be used in the protocol. The `MC_AWAIT` macro can then be used to await some awaitable, e.g. `task<>`. See the [C++14 tutorial](https://github.com/Visa-Research/coproto/blob/master/frontend/cpp14Tutorial.cpp).
```cpp
task<> echoClient(std::string message, Socket& sock) {
    // perform a lambda capture of the parameters
    MC_BEGIN(task<>, message, &sock);
    MC_AWAIT(sock.send(std::move(message)));
    MC_AWAIT(sock.recvResize(message));
    MC_END();
}

task<> echoServer(Socket& sock) {
    // perform a lambda capture of the parameters
    MC_BEGIN(task<>, &sock, message = std::string{});
    MC_AWAIT(sock.recvResize(message));
    MC_AWAIT(sock.send(message));
    MC_END();
}
```


## Build

The library is *cross-platform* and has been tested on Windows, Mac, and Linux. 
If it does not compile, please submit an issue with details.
CMake 3.15+ is required and the helper build script assumes python 3. 
The library can be build directly via cmake or with the `build.py` script.

Build the C++14 library:
```
git clone ...
cd coproto
python3 build.py
```
The main executable with examples is `frontend` and is located in the build directory, eg `out/build/linux/frontend/frontend.exe, out/build/x64-Release/frontend/Release/frontend.exe` depending on the OS.

Build the C++20 library with coroutines:
```
git clone ...
cd coproto
python3 build.py -D COPROTO_CPP_VER=20 
```

### Options
Various options can be set when building the library. These are set via `cmake` or `build.py` with `-D OPTION=VALUE` syntax, e.g. `-D COPROTO_FETCH_AUTO=true`.

#### Fetch Options
* `COPROTO_FETCH_AUTO`: values `true,false`, automatically fetch dependencies if they are not found and needed. Defaults to `false` for cmake and `true` for `build.py`.
* `COPROTO_FETCH_SPAN`: values `true,false`, always fetch the span lite dependencies in C\++14 mode.
* `COPROTO_FETCH_MACORO`: values `true,false`, always fetch the macoro dependencies.
* `COPROTO_FETCH_BOOST`: values `true,false`, always fetch the boost dependencies.
#### Build Options
* `COPROTO_CPP_VER`: values `14,17,20`, build with the desired c++ standard support.
* `COPROTO_ENABLE_BOOST`: values `true,false`, build with boost asio support.
* `COPROTO_ENABLE_OPENSSL`: values `true,false`, build with boost asio OpenSSL support.
* `COPROTO_ENABLE_ASSERTS`: values `true,false`,build with optional asserts enabled.

### Dependencies

The C\++14 version requires [span-line](https://github.com/martinmoene/span-lite), [optional-lite](https://github.com/martinmoene/optional-lite), [variant-lite](https://github.com/martinmoene/variant-lite). Both  C\++14,C\++20 versions depend on [function2](https://github.com/Naios/function2), [macoro](https://github.com/ladnir/macoro) and optionally on [Boost](https://www.boost.org/), [OpenSSL](https://www.openssl.org/). These dependencies can be managed via `CMake`, the `build.py` script or installed via an external tool. If an external tool is used install to system location or set  `-D CMAKE_PREFIX_PATH=path/to/install`. Using the python script, dependencies are automatically pulled based on need. This is achieved by defining the cmake variable `FETCH_AUTO=ON`. 


### Install

Coproto can be installed via cmake or as
```
python3 build.py --install 
```
By default, the dependencies are not installed. To install them, run the corresponding `--setup` command with the `--install` flag.
By default, sudo is not used. If the installation requires sudo access, then add `--sudo` to the `build.py` script arguments. 
Install location can be specified by `--install==path/to/install`.

See `python3 build.py --help` for full details.

### Linking
coproto can be linked via cmake as
```
find_package(coproto REQUIRED)
target_link_libraries(myProject coproto::coproto)
```
To ensure that cmake can find coproto, you can either install coproto or build it locally and set `-D CMAKE_PREFIX_PATH=path/to/coproto` or provide its location as a cmake `HINTS`, i.e. `find_package(coproto HINTS path/to/coproto)`.


