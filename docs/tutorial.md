Introduction
------------

We start by introducing some basic concepts in Hiactor. 
Next, we give an example to illustrate how to define
actors and implement a Hiactor program.

Concepts
-----------

**Actor Model.** 
[Actor model](https://en.wikipedia.org/wiki/Actor_model) is a concurrent programming model 
that uses **Actor** as the primitive computation unit.
Each actor has: (1) a unique address for identification, (2) a mailbox storing the received messages
and (3) a set of predefined methods specifying how the input messages are processed. States are 
private in actors, i.e., an actor cannot modify the state of another actor. Actors communicate with 
each other via asynchronous message passing.


**Shard.** 
Hiactor is built on Seastar, which is an event-driven, sharded framework providing
asynchronous programming abstractions like future/promise and coroutine. 
See this [tutorial](https://github.com/scylladb/seastar/blob/master/doc/tutorial.md#introduction)
for a more detailed introduction of Seastar. Seastar physically split its resources (CPU & memory) 
into multiple shards. Each shard has a piece of memory and is pinned to a CPU core exclusively
by default (can be disabled in configuration). Inheriting the sharded architecture of Seastar,
each shard host a subset of actors (actors are single-threaded), and is responsible for 
scheduling the execution of its local actors. 

In the distributed mode, a Hiactor cluster consists of multiple workers, each of which
has multiple shards. Hiactor assigns each shard a globally unique ID for identification.

Using Hiactor
--------------------------

After Hiactor has been installed (assuming the installation prefix of Hiactor is `$install_prefix`),
you can easily link it with CMake as follows:

```cmake
# Add the cmake prefix path if Hiactor is not installed in a "standard" path like /usr or /usr/local.
list (APPEND CMAKE_PREFIX_PATH $install_prefix)
# Find the Hiactor package
find_package (Hiactor REQUIRED)
```

Defining an Actor
-----------------

The following example illustrates how to implement a dummy bank account actor.
In this example, we define a bank account actor with the account balance 
as its state and three methods: withdraw, deposit and check, which can be used
to manage the account state.

First, Actors and their methods are defined in a `actor_name.act.h` file.

```c++
// file name: bank_account.act.h

#pragma once

#include <hiactor/core/actor-template.hh>
#include <hiactor/util/data_type.hh>

class ANNOTATION(actor:impl) bank_account : public hiactor::actor {
    int balance = 0;
public:
    bank_account(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr) : hiactor::actor(exec_ctx, addr) {
        set_max_concurrency(1); // set max concurrency for task reentrancy
    }
    ~bank_account() override = default;

    /// Withdraw from this bank account by `amount`, return the remaining balance after withdrawing,
    /// if the current balance is not enough, an exception future will be returned.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) withdraw(hiactor::Integer&& amount);

    /// Deposit `amount` into this bank account, return the remaining balance after depositing.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) deposit(hiactor::Integer&& amount);

    /// Return the account balance.
    seastar::future<hiactor::Integer> ANNOTATION(actor:method) check();

    /// Declare a `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};
```

Second, the computation logics of the actor methods should be included in a C++ source file `actor_name.act.cc`.
```c++
// file name: bank_account.act.cc

#include "bank_account.act.h"
#include <seastar/core/print.hh>

seastar::future<hiactor::Integer> bank_account::withdraw(hiactor::Integer&& amount) {
    auto request = amount.val;
    if (request > balance) {
        return seastar::make_exception_future<hiactor::Integer>(std::runtime_error(
                seastar::format("Account balance is not enough, request: {}, remaining: {}.", request, balance)));
    } else {
        balance -= request;
        return seastar::make_ready_future<hiactor::Integer>(balance);
    }
}

seastar::future<hiactor::Integer> bank_account::deposit(hiactor::Integer&& amount) {
    auto request = amount.val;
    balance += request;
    return seastar::make_ready_future<hiactor::Integer>(balance);
}

seastar::future<hiactor::Integer> bank_account::check() {
    return seastar::make_ready_future<hiactor::Integer>(balance);
}
```

Note that there are some rules which should be followed when defining an actor class (in `actor_name.act.h`):
- A customized actor class must be defined in a c++ header file with the suffix `.act.h`.
- The annotation `ANNOTATION(actor:impl)` must be specified in actor class definition.
- A customized actor class must be derived from the base `hiactor::actor` class, the `set_max_concurrency` method
  can be used to set the max [reentrancy concurrency](https://en.wikipedia.org/wiki/Reentrancy_(computing)) of this actor,
  notes that `set_max_concurrency(1)` means disable reentrancy. PS: nested inheritance is allowed:
  you can define an actor class `a` derived from `hiactor::actor` and another actor `b` derived from `a`.
- The constructor parameters of an actor class must be `(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)`
  and should be forward to the base class construction func. These parameters are automatically generated.
  The registration and creation of actors are managed by the Hiactor system and thus transparent to programmers.
- The `do_work` method must be declared with marco `ACTOR_DO_WORK()`. It will be automatically generated, and thus
  no implementation is required.

There are also rules that should followed when implementing an actor (in `actor_name.act.cc`):
- The annotation `ANNOTATION(actor:method)` must be specified in the declaration of each actor method. Any method without
  this annotation will be treated as a helper or private method of actor and cannot be used from a hiactor executing manner.
- The return value of an actor method must be a [future](https://github.com/scylladb/seastar/blob/master/doc/mini-tutorial.md)
  or `void`. Returning `void` means this method returns no result.
- An actor method has at most one parameter (must be a rvalue reference).
- There are some basic template data types included from `hiactor/util/data_type.hh`, programmers
  can also define customized data type. The data type `T` of actor method param `T&& input` or return value
  `future<T>` must implement two methods: `void dump_to(hiactor::serializable_unit &su)` and
  `static T load_from(hiactor::serializable_unit&& su)` for serialization/deserialization.

Actor Reference and Codegen
---------------------------

Actor methods are invoked via **actor reference**. An actor reference acts as a 
client proxy for the corresponding actor. After an actor is defined (see the above example),
its reference class can be automatically generated by the codegen module in Hiactor.
The reference class of actor A has the identical function signatures of all the 
actor methods defined in class A. Calling an actor reference's method will send a specific 
message to the corresponding actor object (either local or remote), and the processing 
result of this message (corresponds to a specific actor method) will be returned
as the result of the reference's method call in the form of `future<T>`.

When we define an actor `bank_account`, Hiactor will automatically generate the codes including
the implementation of `do_work` func and definition of its corresponded actor reference `bank_account_ref`.
After the code generation of `bank_account.act.h` is done, a C++ source file named as
`bank_account.act.autogen.cc` will be generated. This source file includes the generated
implementation of the `do_work` func. In addition, a C++ header file named as
`bank_account_ref.act.autogen.h` will also be generated. This header file defines 
the reference class of the `bank_account` actor. The following lists the 
generated `bank_account_ref` class.

```c++
class bank_account_ref : public hiactor::reference_base {
public:
    bank_account_ref();
    /// actor methods
    seastar::future<hiactor::Integer> withdraw(hiactor::Integer&& amount);
    seastar::future<hiactor::Integer> deposit(hiactor::Integer&& amount);
    seastar::future<hiactor::Integer> check();
};
```

Hiactor provides a cmake function to use its codegen tools:

```cmake
# import the actor codegen cmake file
include ($install_prefix/hiactor_codegen/ActorAutoGen.cmake)
# actor codegen cmake func
# @param "actor-autogen": specify the cmake target for actor codegen.
# @param "actor_gen_files": specify the generated actor definition files.
# @param "SOURCE_DIR": set the source dir that contains use-defined actor files.
# @param "INCLUDE_PATHS": set the include directories to search headers with a comma-separated list.
hiactor_codegen (actor_autogen actor_gen_files
  SOURCE_DIR $my_app_dir
  INCLUDE_PATHS $install_prefix/include,$my_app_dir)
```

Actor Addressing
-----------------------------

Each actor has a globally unique address. Building an actor reference refering to
a specific actor only requires its address. Different from other actor systems
like [Orleans](https://github.com/dotnet/orleans) and [CAF](https://github.com/actor-framework),
Hiactor manages actors in a hierarchical structure to facilitate features such as
resource isolation and customized scheduling in arbitrary scopes. We propose a 
an abstraction referred to as **actor group** for this purpose. Actor group is a
special type of actor, it functions as the *execution context* of other actors(actor groups).
An actor can only be scheduled by its parent actor group. When an actor group is 
scheduled, it further schedules its child actors(actor groups). This tutorial(TODO)
illustrates how to use actor group to enforce performance isolation among concurrent 
queries and implement customized scheduling policies to flexibly control the execution
of actors.

The address of an actor consists of three parts: a global shard id, the ids of its
ancestoring actor groups in sequence and its local actor id. Note that the local actor
id is globally unique in the scope identified by the shard id and the sequence of actor
group ids. Hiactor provides a *scope builder* to help programmers create actor references 
in an intuitive approach.

```c++
seastar::future<> simulate() {
    hiactor::scope_builder builder;
    auto ba_ref = builder
        .set_shard(0)
        .enter_sub_scope(hiactor::scope<hiactor::actor_group>(1))
        .enter_sub_scope(hiactor::scope<hiactor::actor_group>(2))
        .build_ref<bank_account_ref>(10);
    return ba_ref.deposit(hiactor::Integer(5)).then([] (hiactor::Integer&& balance) {
        fmt::print("Successful deposit, current balance: {}\n", balance.val);
    });
}
```

The above program illustrates a program that deposits `10` in a specific bank account actor.
We use scope builder to create the actor reference, where the actor is located on
shard `0` with actor id `10`, managed by actor group `1` and actor group `2` hierarchically.
After creating the `ba_ref`, we can call its `deposit` method and print the returned balance
after the depositing is completed.


Writing a Hiactor Program
-------------------------

After defining actors and code generation, we can write a Hiactor program like this:

```c++
// file name: main.cc

#include "generated/bank_account_ref.act.autogen.h"
#include <hiactor/core/actor-app.hh>
#include <seastar/core/print.hh>

seastar::future<> simulate() {
    hiactor::scope_builder builder;
    auto ba_ref = builder
        .set_shard(0)
        .enter_sub_scope(hiactor::scope<hiactor::actor_group>(1))
        .enter_sub_scope(hiactor::scope<hiactor::actor_group>(2))
        .build_ref<bank_account_ref>(10);
    return ba_ref.deposit(hiactor::Integer(5)).then([] (hiactor::Integer&& balance) {
        fmt::print("Successful deposit, current balance: {}\n", balance.val);
    });
}

int main(int ac, char** av) {
    hiactor::actor_app app;
    app.run(ac, av, [] {
        return simulate().then([] {
            hiactor::actor_engine().exit();
            fmt::print("Exit hiactor system.\n");
        });
    });
}
```

As this example, a hiactor program starts by creating an `actor_app` object.
This object starts the hiactor engine and then runs the given lambda function.
Call `actor_engine().exit()` to stop the hiactor engine after the lambda
function is completed.

The following steps show how to build a Hiactor program.
Suppose the directory of the example project is as follows:

```
$my_app_dir
｜ actor
｜ ｜ bank_account.act.h
｜ ｜ bank_account.act.cc
｜ main.cc
｜ CMakeLists.txt
```

Given the following CMakeLists.txt:
```cmake
list (APPEND CMAKE_PREFIX_PATH $install_prefix)

find_package (Hiactor REQUIRED)

include ($install_prefix/hiactor_codegen/ActorAutoGen.cmake)

hiactor_codegen (actor_autogen actor_gen_files
  SOURCE_DIR $my_app_dir
  INCLUDE_PATHS $install_prefix/include,$my_app_dir) 

add_executable (bank_account_example
  main.cc
  actor/bank_account.act.cc
  actor/bank_account.act.h
  ${actor_gen_files})

# Disable attribute warning
target_compile_options(bank_account_example
  PRIVATE -Wno-attributes)

add_dependencies (bank_account_example
  actor_autogen)

target_link_libraries (bank_account_example
  PRIVATE Hiactor::hiactor)
```

Compile the example program with the following commands:
```shell
$ mkdir build && cd build
$ cmake ..
$ make
```

Run the program:
```shell
$ ./bank_account_example
Successful deposit, current balance: 5
Exit hiactor system.
$
```

The complete bank account demo can be found [here](../demos/bank_account).