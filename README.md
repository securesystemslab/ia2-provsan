# Provsan

ProvSan is a course grained dataflow analysis tool that tracks object provenance to understand cross-compartment data flow.
It is used to debug hard to track data flow problems when Intel MPK is used as the isolation primative between compartments.
When compiling with ProvSan, custom compiler passes add runtime instrumentation to track heap allocations. 
When a compartment violation occurs, it will trigger a memory access violation, which will be serviced by our runtime component.
The Runtime will log the fault, and which allocation site triggered it.

After the sanitizer runs, the collected profiles are fed back into the compiler, and all the access violations in the profiles are reported.


We support default clang/llvm toolchains, with out of tree passes, that are loadable by a standard clang compiler.

## Collecting Profiles with ProvSan
Compile your project with additional compile and link flags to correctly build using provsan

Below we add the LLVMProvsanPre.so and LLVMProvsanPost.so as pass plugins to clang. This allows our passes to run as part of normal compilation
Additionally, since this command is makeing an executable, we pass the libprovsan_rt.so library as a dependency and set the rpath acordingingly.
Lastly, for good reporting we compile with debugsymbols, so that our reports can give source code information.

The other important information you need compile in profiling mode is to know how to set environment variables for the compartment allocators.
Since LLVM-12, llvm pass plugins are no longer loadable if they are not compatable with the new-pm. However, the new-pm does not register commandline flags early enough for plugins options to be parsed by clang and correctly passed to the llvm backend. To work around this we use a set of environment variables to configure how our passes and reporting should behave.

For profiling the following flags control which allocation APIs to hook from the trusted compartment.
  - PROVSAN_ALLOC - a comma separated list of allocation APIs. In the example below we pass `trusted_malloc` and `foo` this way
  - PROVSAN_REALLOC - a comma separated list of reallocation APIs. In the example below we pass `trusted_realloc`  this way
  - PROVSAN_FREE - a comma separated list of deallocation APIs. In the example below we pass `trusted_free` this way
```
$ PROVSAN_ALLOC="trusted_malloc,foo" PROVSAN_REALLOC=trusted_realloc PROVSAN_FREE=trusted_free clang -fpass-plugin=/path/to/LLVMProvsanPre.so -fpass-plugin=/path/to/LLVMProvsanPost.so -fexperimental-new-pass-manager /path/to/libprovsan_rt.so -Wl,-rpath,/path/to/provsan/Runtime/build -g -flto -O2
```


## Using Profiles
After profiling the application on some test inputs, there will be a set of profiles in a `TestResults` folder.
Each profiling run will log all of the allocation site metadata to a JSON file, that the compiler passes can consume to generate a report

We add 2 new environment variables:
 - PROVSAN_PATH - the path to the TestResults folder
 - PROVSAN_HOOK - which tells the pass to remove the provsan instrumentation
 
When PROVSAN_PATH is non-empty, ProvSan will generate a report of the cross-compartment violations it found.

```
$ PROVSAN_ALLOC="trusted_malloc,foo" PROVSAN_REALLOC=trusted_realloc PROVSAN_FREE=trusted_free PROVSAN_PATH=$PWD/TestResults PROVSAN_HOOK=1 clang -fpass-plugin=/path/to/LLVMProvsanPre.so -fpass-plugin=/path/to/LLVMProvsanPost.so -fexperimental-new-pass-manager /path/to/libprovsan_rt.so -Wl,-rpath,/path/to/provsan/Runtime/build -g -flto -O2 -pthread -lstdc++
```

Example output:
```
Error: Compartment Violation from memory originally allocated at basic.c:48:25
```


## Acknowledgements

This material is based upon work partially supported by the
Defense Advanced Research Projects Agency (DARPA) under
contracts W31P4Q-20-C-0052 and W912CG-21-C-0020. Any
opinions, findings, and conclusions or recommendations ex-
pressed in this material are those of the authors and do not
necessarily reflect the views of the Defense Advanced Re-
search Projects Agency (DARPA), its Contracting Agents, or
any other agency of the U.S. Government. We also thank the
Donald Bren School of Information and Computer Science
at UCI for an ICS Research Award.
