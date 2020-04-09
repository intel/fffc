Function Fuzzer for C
=====================

FFFC is an automagical API fuzzer generator for C-language programs. What does
that mean really?

* FFFC takes a test program compiled with a few near-universally available
  debugging options as input.
* It then generates mutator functions for the various types that binary uses, eg
  structs, unions, typedefs, etc.
* It bundles those mutators up to build a set of fuzzers which each intercept
  and mutate arguments for a specific function in the test program.
* These programs can then be run over the test program to look for bugs without
  having to write any boilerplate.

How do you use it?
------------------
Using FFFC is simple. Let's walk through it step-by-step.

### 0. Meet the platform requirements
FFFC can only run on modern x86_64 Linux boxes, and only on userspace code. You
should also disable ASLR when running the target, either by turning it off
globally or by running in a shell with it turned off, eg:

```console
user@host:~$ setarch `uname -m` -R /bin/sh
user@host:~$
```

Note that this is a security risk. You should not do this in production. You can
also globally disable ASLR via:

```console
root@host:~$ echo 0 > /proc/sys/kernel/randomize_va_space
root@host:~$
```

### 1. Install dependencies
FFFC requires python3, pyelftools, pycparser, libsubhook, and a semi-modern
compiler. An appropriate version of pyelftools and pycparser will be
automatically installed for you, but libsubhook will need to be installed
manually. You can get it from https://github.com/Zeex/subhook. For more details
on this process, see INSTALL.md.

### 2. Install FFFC
In the same directory as this README, simply run "sudo python3 setup.py
install".

### 3. Compile your binary
FFFC needs more debugging information than is available in stripped and
optimized binaries, but not too much more. To the best of our knowledge every
version of Clang and GCC going back to versions 3.5 and 4.8 respectively (ie,
within the last 5 years) fully support everything FFFC needs.

How do we add that info? Through additional compiler flags. Specifically:

* Debug info: FFFC requires that programs be compiled with -g to retain DWARF
  debugging information.
* Unoptimized: FFFC prefers to work on programs at -O0, although some may work
  at eg -01 or even higher.
* With AddressSanitizer: FFFC requires -fsanitize=address to get runtime size
  information. For Clang, it also requires -shared-libasan.
* Profiling: FFFC requires -fprofile-arcs.

Some additional flags can help produce higher quality output:

* -fno-common: With this flag set, ASAN can instrument (and therefore FFFC can
  mutate) uninitialized global arrays.
* -fno-omit-frame-pointer: Allows ASAN's fast unwinder to work correctly.

Putting this all together, a program which is normally compiled like this:

~~~
clang -o test.clang test.c
~~~

Would be compiled like this instead:

~~~
clang -o test.clang -g -O0 -fsanitize=address -shared-libasan -fprofile-arcs -fno-common -fno-omit-frame-pointer test.c
~~~

And one compiled like this:

~~~
gcc -o test.gcc test.c
~~~

Would become:

~~~
gcc -o test.gcc -g -O0 -fsanitize=address -fno-common -fno-omit-frame-pointer test.c
~~~

### 4. Run FFFC

It all gets easier from here. To run FFFC over the generated binary is simple:

~~~
fffc <binary> <output_dir>
~~~

So in the above example it would be something like:

~~~
fffc test.gcc /tmp/out
~~~

FFFC will create the output directory for you if it doesn't exist.

### 5. Running the fuzzers

In step 4, FFFC generated a set of fuzzers for you and compiled them. In this
step we'll actually run them.

Let's imagine that the test.c file we mentioned earlier looked something like
this:

```c
// test.c

#include <stdio.h>

int f(int x) {
	printf("f(%d)\n", x);
	return x*x;
}

int g(int x) {
	printf("g(%d)\n", x);
	return x+1;
}

int main(void) {
	return f(g(f(g(0))));
}
```

Not a very inspiring example, but that's fine for now (and this example is
checked in under fffc/tests/doctest). If you compiled this and ran FFFC over it
as described above you would see output like the following:

```console
user@host:~$ fffc test.gcc /tmp/out
Not fuzzing non-C compile unit; continuing...
Skipping 0-argument function main() ...
/tmp/out/test.gcc/g.c
/tmp/out/test.gcc/f.c
Generating /tmp/out/test.gcc/g_runner.sh ...
Generating /tmp/out/test.gcc/f_runner.sh ...
Generating /tmp/out/test.gcc/g_rebuild.sh ...
Generating /tmp/out/test.gcc/f_rebuild.sh ...
Generating /tmp/out/test.gcc/g_debug.gdb ...
Generating /tmp/out/test.gcc/g_debug.sh ...
Generating /tmp/out/test.gcc/f_debug.gdb ...
Generating /tmp/out/test.gcc/f_debug.sh ...
user@host:~$ 
```

As you can see, it has generated a set of scripts for you. Each of these scripts
helps set you up to run the fuzzer or investigate crashes it generates. For now,
we'll focus on the runners.

Runners provide a way to execute your binary in such a way that FFFC will
intercept calls of the named function (g in the case of the script named
g_runner and f in the case of f_runner) and mutate its arguments before calling
the real function. So, to test the function f() from the code above, we would do
this:

```console
user@host:~$ FFFC_LOG_LEVEL=INFO /tmp/out/test.gcc/f_runner.sh
Using user-provided log level INFO.
Using default resize rate; set via FFFC_RESIZE_RATE environment variable.
Using default mutation rate; set via FFFC_MUTATION_RATE environment variable.
Using default mutation count; set via FFFC_MUTATION_COUNT environment variable.
Using default generation count; set via FFFC_GENERATION_COUNT environment variable.
Using default crash path; set via FFFC_CRASH_PATH environment variable.
Using default working path; set via FFFC_DATA_PATH environment variable.
Using default state size; set via the FFFC_MAX_STATE_COUNT environment variable
Fuzzing normally; to replay a specific run, set the FFFC_DEBUG_REPLAY=<logfile> environment variable
g(0)
Executions per second: 3892
Executions per second: 982
Executions per second: 1061
Executions per second: 1077
Executions per second: 1084
...
```

There's quite a bit to unpack here, but you can really think of this as three
pieces of output:

The first block is just telling you that you're using the default settings for
FFFC and how to adjust them; you can ignore it for now. The second part ("g(0)")
is the output from where the program is executing as it normally does. And the
third block is a sequence of lines telling you how many fuzz runs FFFC is doing
on f() every second. At this point, FFFC is fuzzing your program!

Eventually, you will see more messages from the normally-executing program,
something like this:

~~~
f(1)
g(1)
Executions per second: 3874
...
~~~

That's because, unlike many other fuzzers, the loop I describe above will not go
on forever. It repeats a certain number of times and then permits the program to
continue executing in the hope of seeing a new seed input that explores other
interesting state. Eventually, assuming the target program ever terminates, so
will the fuzzer.

In this case, once the program is done fuzzing the first (inner) invocation of
f(), it allows the program to continue until the outer invocation is reached. It
then begins fuzzing that. If you want to get to that point faster, you can
reduce the number of times the fuzzer executes by doing something like this:

```console
user@host:~$ FFFC_GENERATION_COUNT=1 FFFC_LOG_LEVEL=INFO /tmp/out/test.gcc/f_runner.sh 
Using user-provided log level INFO.
Using default resize rate; set via FFFC_RESIZE_RATE environment variable.
Using default mutation rate; set via FFFC_MUTATION_RATE environment variable.
Using default mutation count; set via FFFC_MUTATION_COUNT environment variable.
Using user-provided generation count: 1
Using default crash path; set via FFFC_CRASH_PATH environment variable.
Using default working path; set via FFFC_DATA_PATH environment variable.
Using default state size; set via the FFFC_MAX_STATE_COUNT environment variable
Fuzzing normally; to replay a specific run, set the FFFC_DEBUG_REPLAY=<logfile> environment variable
g(0)
Executions per second: 3940
f(1)
g(1)
Executions per second: 3927
f(2)
user@host:~$
```

### 6. Next steps: looking at saved program states

When a crash occurs FFFC saves information about the crash and how it got there
into a crash directory, by default located in the current working directory. The
information it saves includes the standard output and standard error from the
program, a log of the changes FFFC made, and the ASAN crash output. After an
FFFC run, you can use FFFC's builtin `fffc_dedup_crashes` tool to deduplicate
those crashes, and you can even jump into the program at the point where a crash
occurs using the \*\_debugger.sh scripts generated at the same time as the
\*\_runner.sh scripts you ran earlier.

Although the program above does not crash, we can get an idea about how to
explore crashes by looking at what FFFC has generated for it. Assuming you
executed as above, you will see a pair of directories in your cwd named
something like "fffc_state.f.$TIMESTAMP.xxxxxx" and
"fffc_crashes.f.$TIMESTAMP.xxxxxx". Looking inside, we will see a further set of
subdirectories for each time the target was called, named things like
"00000001.xxxxxx", "00000002.xxxxxx", and so on. Inside of these we see saved
state for each individual invocation of the function-- in particular, coverage
data, stderr and stdout, and the FFFC mutation log:

```console
user@host:~$ ls fffc_state.f.$TIMESTAMP.xxxx/00000001.xxxx/f-4030
coverage  log  stderr  stdout
```

In the case of a crash, you will also see the ASAN output for the target
program. Besides seeing information about what the program output and similar,
we can use this to exactly reexecute the mutations which FFFC applied to aid in
debugging crashes:

```console
user@host:~$ FFFC_DEBUG_REPLAY=fffc_state.f.$TIMESTAMP/00000001.xxxx/f-4030/log /tmp/out/test.gcc/f_debug.sh
```

This will fire up GDB, execute through the binary until it finds the correct
call to the target function, apply the relevant mutations, and then break so
that you can interact with the program.

There is also a tool for helping you navigate these crashes, called
`fffc_crashtool`. The typical usage is something like this:

```console
user@host:~$ export FFFC_CRASH_PATH=/tmp/crashes
user@host:~$ /tmp/out/test.gcc/f_runner.sh
user@host:~$ . fffc_crashtool /tmp/crashes/*
user@host:~$ /tmp/out/test.gcc/f_debug.sh
```

This will run your test, then let you page through the different crash logs
until you find one you're interested in. Once you find it, simply crtl-c to exit
and it will automatically populate FFFC_DEBUG_REPLAY for you so that the debug
script is ready to run.


### 7. Next steps: interacting with FFFC via environment variables

FFFC supports additional environment variables to control behavior like the
mutation rate, resize rate, queue size, and other metaparameters. These are
called out at the beginning of a fuzz run and can be important to tune for some
use cases. In particular, if used as part of a CI system you will probably want
to reduce the FFFC_GENERATION_COUNT or FFFC_MUTATION_COUNT to decrease execution
time. And odds are good you will want better control over where the program
state and crashes are stored: this can be changed via FFFC_CRASH_PATH and
FFFC_DATA_PATH.

### 8. Next steps: custom behavior

FFFC's built-in mutators are intended to stress many real-life applications but
don't know specifically how your data is used. As a result, you may want to
change its behavior to better suit your application. Doing this is supported via
FFFC's rebuild scripts.

For example, let's say you have a program like this:

```c
// test2.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int f(char *x, char *unused) {
	if (*x != 'X') {
		exit(-1);
	}
	printf("%s\n", x);
	return 0;
}

int main(void) {
	char *arg = strdup("hello!");
	char *arg2 = strdup("supercalifragilisticexpialidocious(sp)");
	return f(arg, arg2);
}
```

As you can see, the target function takes two arguments-- but one of them is
unused and the other has a constraint on it that will cause us to waste a lot of
time. We can improve the efficiency of the fuzzer by accounting for these facts.

To start, we build the fuzzers just like we would normally:

```console
user@host:~$ gcc -g -O0 -fsanitize=address -fprofile-arcs -lm -fno-common test2.c -o test2.gcc
user@host:~$ fffc test2.gcc /tmp/out
Not fuzzing non-C compile unit; continuing...
/tmp/out/executables/test2.gcc/f.c
Generating /tmp/out/test2.gcc/f_runner.sh ...
Generating /tmp/out/test2.gcc/f_rebuild.sh ...
Generating /tmp/out/test2.gcc/f_debug.gdb ...
Generating /tmp/out/test2.gcc/f_debug.sh ...
user@host:~$
```

Now instead of running it, we're going to modify the fuzzer's behavior. If we
now look into the output directory, we can see that in addition to the runners a
number of .c files were generated. The one of interest to us is simply the
function's name dot c-- so in this case, f.c. This is the main runtime for FFFC
when it's fuzzing the function f().

There's a lot in there, but you can see a number of places where FFFC invokes a
set of mutators functions. Places like this:

```c
fffc_restrict_child();
fffc_replay_log();
if (!fffc_debug()) {
    /* int fffc_mutator_for_target_type(char **storage)*/
    _Z_fffc_mutator_4929E2A5BF22A0D8(&_x);
    /* int fffc_mutator_for_target_type(char **storage)*/
    _Z_fffc_mutator_4929E2A5BF22A0D8(&_unused);
}
fffc_precall();
int retval = FFFC_target(_x, _unused);
fffc_postcall();
fffc_exit_child();
```

Here you can see that FFFC is calling the name-mangled mutator functions on two
variables called \_x and \_unused. As you can see from the above, these are the
arguments to our function. To avoid wasting time fuzzing the unused argument, we
can simply remove the calls which mutated it. That leaves us with something like
this:

```c
fffc_restrict_child();
fffc_replay_log();
if (!fffc_debug()) {
    /* int fffc_mutator_for_target_type(char **storage)*/
    _Z_fffc_mutator_4929E2A5BF22A0D8(&_x);
}
fffc_precall();
int retval = FFFC_target(_x, _unused);
fffc_postcall();
fffc_exit_child();
```

We can then rebuild the runner by invoking the \*\_rebuild.sh script:

```console
user@host:~$ /tmp/out/test2.gcc/f_rebuild.sh
user@host:~$
```

And then run it as usual:

```console
user@host:~$ /tmp/doctest/executables/test.gcc/f_runner.sh
Couldn't get parent log; this is probably due to excessive crashes.
Couldn't get parent log; this is probably due to excessive crashes.
Couldn't get parent log; this is probably due to excessive crashes.
Couldn't get parent log; this is probably due to excessive crashes.
Couldn't get parent log; this is probably due to excessive crashes.
...
```

The good news is that now we aren't wasting time fuzzing a variable that won't
be used, but we're still hitting that value check. Let's try again:

```c
fffc_restrict_child();
fffc_replay_log();
if (!fffc_debug()) {
    /* int fffc_mutator_for_target_type(char **storage)*/
    _Z_fffc_mutator_4929E2A5BF22A0D8(&_x);
	_x[0] = 'X';
}
fffc_precall();
int retval = FFFC_target(_x, _unused);
fffc_postcall();
fffc_exit_child();
```

Now we're just setting the first byte to 'X' no matter what. Doing this will
quickly reduce the number of crashes, but risks writing to a null pointer or
zero byte allocation. We could ignore those issues and hope that such a
situation never occurs, or we could modify the mutator itself. To do that, we
can just grep for the mutator signature before it was mangled, as you can see in
the comment above the mutator:

```console
user@host:/tmp/out/test2.gcc$ grep -r 'int fffc_mutator_for_target_type(char \*\*storage)' .
./0xdf8_test2_mutator.c:/* int fffc_mutator_for_target_type(char **storage)*/
./f.c:			/* int fffc_mutator_for_target_type(char **storage)*/
./f.c:		/* int fffc_mutator_for_target_type(char **storage)*/
./f.c:		/* int fffc_mutator_for_target_type(char **storage)*/
./mutator.h:/* int fffc_mutator_for_target_type(char **storage)*/
```

The one we want is in the file named "0xdf8_test2_mutator.c"; the others are
just comments at the point of invocation or declaration. In there we will find
something like this:

```c
/* int fffc_mutator_for_target_type(char **storage)*/
int _Z_fffc_mutator_45125D38C54DC4C1(char **storage)
{
  long long int size = fffc_estimate_allocation_size((void *) (*storage));
  unsigned long long int member_size = fffc_get_sizeof_03E77EA90C6D13A9(*storage);
  size = fffc_maybe_munge_pointer((unsigned char **) storage, size, member_size);
  if (size < 0)
  {
    return 0;
  }

  void *data = (void *) (*storage);
  while (size > member_size)
  {
    _Z_fffc_mutator_1E29168D605073DB(data);
    data += member_size;
    size -= member_size;
  }

  if (member_size != 1)
  {
    _Z_fffc_mutator_1E29168D605073DB(data);
  } else if (*(unsigned char*)data != 0) {
    _Z_fffc_mutator_1E29168D605073DB(data);
  }

  return 0;
}
```

This is the mutator for the target type (char\*\*). As you can see it knows the
size of the allocation, the size of a char, and has access to the data for it.
As a result we can decide whether we want to hardcode the 'X' into place, just
skip the first byte, or whatever else we want to do. It can also be useful to
simply copy the mutator function, rename it appropriately, and invoke it rather
than the default in the runtime. This avoids changing the behavior of all char\*
mutators when the goal is just to sidestep one check or improve efficiency in
one place. A good example of when this is useful is with the `member_size != 1`
etc check: this is in place because so many C programs misbehave when presented
with non-null-terminated strings, but it is also often useful to fuzz the last
byte even if it is already null in all inputs.

One thing that is often useful for maintaining a set of modified mutators is to
disable Python's default randomized hashing behavior, which will change the
names of the mutators on each execution. You can do this via:

```console
user@host:$ export PYTHONHASHSEED=0
```

Common questions
----------------

Q: How does FFFC compare with AFL?

A: FFFC is a very different kind of tool from AFL. Where AFL is mostly
   focused on programs that already have a fuzzer-friendly API (such as taking a
   file as input on the command line), FFFC concentrates on programs that don't.
   There are a few upsides to this approach and a few downsides. On the positive
   end, FFFC can quickly be gotten running on codebases that otherwise would
   require a lot of handwritten serialization and deserialization code to
   integrate with AFL and other fuzzers, meaning that often the 'time to first
   fuzz' for FFFC is shorter. It also understands more about what the program
   intends to do with a given input, which can help it find ways to violate
   common assumptions more quickly. On the other hand, AFL is very fast, very
   well-tested, and works on more platforms than FFFC does. AFL also maps well
   to true attack scenarios where an attacker may not be able to pass
   fully-arbitrary input to your API, where FFFC maps more readily onto testing
   scenarios where you wish to identify ways to make your code more robust
   without necessarily developing an exploit for it.

Q: Does FFFC make a new process for every call of the target?

A: Yes. FFFC executes each invocation in its own process to try to keep the
   original process environment clean. Of course, this is a best-effort
   situation; programs which create files or talk over the network will probably
   get confused sooner than their less talkative counterparts.

Q: Does FFFC mutate global state?

A: Not knowingly, currently. FFFC has enough information to do this on
   purpose, but not necessarily efficiently. It is also possible for a target
   function to mutate global state, then have another execution of that function
   occur inside the same process. In practice this is a fairly reachable way to
   corrupt global state, and so is something we intentionally model-- but may
   someday add a switch to disable for one-shot environments.

Q: Does FFFC interleave function executions?

A: Not currently, although it could easily be adapted to do so.

Q: Why does FFFC require debugging info?

A: FFFC attempts to mutate data in a type-aware way rather than simply
   randomly permuting it. For example, imagine a function which takes a single
   char\* as an argument. Flipping bits in the passed-in pointer would just
   yield a wide variety of wild pointers, when what we really want is to fuzz
   the text it points to. Debugging information is what tells us that the
   function takes that single argument and that it is a pointer, and enables us
   to build mutators which take advantage of that fact.

Q: Why does FFFC require ASAN?

A: FFFC leverages ASAN's memory tracking to figure out how large an
   allocation an array or pointer has behind it. This is the other enabling
   technology behind the example above: without it FFFC might be able to guess
   the size of a char\* by hoping it is a properly null-terminated C string, but
   it would have no idea how many ints might be on the far side of an int\*.

Q: Why does FFFC require -O0?

A: Both your code and the corresponding debugging output are made much
   simpler by forbidding common optimizations. While there isn't any guarantee
   that FFFC *won't* work at -O3 on your particular program, odds are pretty
   good.
