
basic_prof:
	clang -fpass-plugin=/home/paul/workspace/ia2/ia2-phase2/provsan/Passes/build/DynUntrustedAllocPre/LLVMDynUntrustedAllocPre.so -fpass-plugin=../../Passes/build/DynUntrustedAllocPost/LLVMDynUntrustedAllocPost.so basic.c -fexperimental-new-pass-manager -O1 ../../Runtime/build/libprovsan_rt.so -Wl,-rpath,$PWD/../../Runtime/build -g -lstdc++ -pthread

%.ll: %.c
	$(CC) -emit-llvm -S -Xclang -disable-O0-optnone

%.bc: %.c
	$(CC) -emit-llvm -Xclang -disable-O0-optnone


