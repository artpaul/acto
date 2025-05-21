set(LLVM_ROOT /usr/lib/llvm-18)
set(CMAKE_C_COMPILER ${LLVM_ROOT}/bin/clang)
set(CMAKE_CXX_COMPILER ${LLVM_ROOT}/bin/clang++)
set(CMAKE_EXE_LINKER_FLAGS "--ld-path=${LLVM_ROOT}/bin/ld.lld")
