 Hiactor
=======

What is Hiactor
------------

Hiactor is an open-source hierarchical [actor](https://en.wikipedia.org/wiki/Actor_model)
framework for building high-performance，concurrent and scalable event-driven
systems using C++. Hiactor features a light-weight and ease-of-use distributed implementation
of the actor model with concepts like *object*, *reference* and *future/promise*.
With Hiactor, developers familiar with single-server programming can experience
a seamless transition to the distributed environments.


Building Hiactor
----------------

Requirements:

- A compiler with good C++17 support (e.g. gcc >= 9.0). This was tested successfully on g++ 9.4.0.
- A higher version linux system is recommended. This was tested successfully on Ubuntu 20.04.
- CMake >= 3.13.1

Hiactor has a dependency of [Seastar](https://github.com/scylladb/seastar).
Before building Hiactor, update the project submodules and install dependencies
for Seastar:

```shell
$ git submodule update --init --recursive
$ sudo ./seastar/seastar/install-dependencies.sh
```

Next, build Hiactor as follows:

```shell
$ mkdir build
$ cd build
$ cmake [OPTIONS] ..
$ make
$ make install
```

The following cmake options can be specified:
* `Hiactor_DEBUG`: Build Hiactor with debug mode. Default is `OFF`.
* `Hiactor_INSTALL`: Enable installing Hiactor targets. Default is `ON`.
* `Hiactor_DEMOS`: Enable demos of Hiactor. Default is `ON`.
* `Hiactor_TESTING`: Enable tests of Hiactor. Default is `ON`.
* `Hiactor_GPU_ENABLE`: Enable gpu devices for Hiactor (Cuda environments required!) Default is `OFF`.
* `Hiactor_DPDK`: Enable DPDK support for Hiactor. Default is `OFF`.
* `Hiactor_CXX_DIALECT`: Specify the C++ dialect for Hiactor. Default is `gnu++17`.
* `Hiactor_CXX_FLAGS`: Specify other compilation flags for Hiactor.
* `Hiactor_COOK_DEPENDENCIES`: Whether to download and build Seastar's dependencies for use if you don't
want to use system packages (RPMs or DEBs). Default is `OFF`.
* `Hiactor_COOK_EXCLUDES`: A semicolon-separated list of dependency names to exclude from building if
`Hiactor_COOK_DEPENDENCIES` is enabled.
* `Hiactor_CPU_STALL_REPORT`: Enable warning reports at cpu stalls. Default is `OFF`.
* `Hiactor_UNUSED_RESULT_ERROR`: Make [[nodiscard]] violations an error (instead of a warning). Default is `OFF`.

Seastar is embedded with `add_subdirectory` in hiactor, you can add
cmake options of Seastar directly when configuring. Note that the apps,
demos, docs and tests of Seastar are disabled in its embedding mode,
besides, `Seastar_CXX_DIALECT` will be overridden by `Hiactor_CXX_DIALECT`
and `Seastar_DPDK` will be overridden by `Hiactor_DPDK`.

An example cmake options:

```shell
$ cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DHiactor_CXX_DIALECT=gnu++17 \
    -DHiactor_TESTING=OFF -DSeastar_CXX_FLAGS="-DSEASTAR_DEFAULT_ALLOCATOR" ..
```

Getting started
---------------

Follow the [tutorial](docs/tutorial.md) to understand how to use Hiactor.