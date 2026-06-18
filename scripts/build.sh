#!/bin/bash
set -e

########################################parameters########################################
readonly BUILD_SCRIPT="${0}"
readonly INSTALL="linx_blockisa_llvm"
readonly PREFIX="$PWD/$INSTALL"
readonly BUILD_OBJ="$PWD/build_obj"
readonly SRC_PATH="$PWD/.."
readonly BINUTILS_GIT_NAME="linx-BLK-binutils"
readonly LLVM12_GIT_NAME="linx-llvm"
readonly BINUTILS_SRC_PATH="$PWD/../${BINUTILS_GIT_NAME}"
readonly LLVM_SRC_PATH="$PWD/../${LLVM12_GIT_NAME}"
readonly OUTPUT="$PWD/../output"

COMPILER_INFO="Linx Block ISA VXXXRXXXCXXBXXX"
LLVM_BUILD_TYPE="Release"
LLVM_ASSERTIONS="ON"
LLVM_TARGETS_TO_BUILD="RISCV;X86;AArch64"
LLVM_ENABLE_PROJECTS="clang"
LLVM_LINK_LLVM_DYLIB="ON"
readonly -A GIT_PATH=([${BINUTILS_GIT_NAME}]="-b master ssh://contributor:2222/linx/ISA-Codesign/BlockISA/${BINUTILS_GIT_NAME}.git"
                      [${LLVM12_GIT_NAME}]="-b dev-gfu ssh://contributor:2222/linx/ISA-Codesign/BlockISA/${LLVM12_GIT_NAME}.git")
llvm_version="llvm12.0"
code_pull_mode=0
llvm_debug_mode=1
number_compile_task=32
build_project="all"
#build_project_name="llvm binutils glibc"
build_project_name="llvm binutils"

###########################################functions######################################
use_bepkit() {
  source $BEP_ENV_PATH/bep_env.sh -s $PWD/bep_env.conf
}

help_info() {
  echo
  echo "Usage: ${BUILD_SCRIPT} [-v <llvm_version>] [-u <update_code_mode>] [...]"
  echo "Options:"
  echo "  -h, --help              Print the help message of ${BUILD_SCRIPT} and exit."
  echo "  -v, --version           Select the version of llvm, value is one of: llvm10.0, llvm12.0. Default is llvm12.0."
  echo "  -u, --update            Select the mode of updating the code, value is one of: 0, 1, 2. Default is 0.
                                    0 -- Not update the code if the code repository exists.
                                    1 -- Update the code.
                                    2 -- Re-pull the all code."
  echo "  -d, --debug             Set the debug mode for llvm, value is one of: 0, 1, 2. Default is 1.
                                    0 -- -DCMAKE_BUILD_TYPE=Release, build release version llvm.
                                    1 -- -DCMAKE_BUILD_TYPE=Release && -DLLVM_ENABLE_ASSERTIONS=ON,
                                         build release version llvm which enables code assertions.
                                    2 -- -DCMAKE_BUILD_TYPE=Debug, build debug version llvm."
  echo "  -n, --number            Set the number of parallel compilation tasks (ninja -j n, make -j n). Default is 32."
  echo "  -b, --build             Set the project of linx server toolchain which you want to build,
                                    value is in: llvm, binutils, glibc, all. Default is all.
                                    Note: the linx server toolchain will be packaged to output dir only -b all.
                                    Example: -b llvm --- only build llvm project.
                                             -b llvm-binutils --- build llvm and binuitls project.
                                             -b all --- build all project in linx server toolchain."
  echo "  -l, --linx              Set the version number of linx server toolchain.
                                    Default is invalid version number: \"Linx Base Software-Server VXXXRXXXCXXBXXX\""
  echo
  echo "Example: ./build.sh -v llvm12.0 -u 2 -d 0 -n 64 -b all -l \"Linx Block ISA VXXXRXXXCXXBXXX\""
  echo
}

get_opt() {
  while [ -n "$1" ]
    do
      case "$1" in
        -v|--version)
            llvm_version="$2"
            if [ ${llvm_version} != "llvm10.0" ] && [ ${llvm_version} != "llvm12.0" ]; then
              echo "Error: the value of llvm version is incorrect."
              exit 1;
            fi
            shift ;;
        -u|--update)
            code_pull_mode="$2"
            if [ ${code_pull_mode} != 0 ] && [ ${code_pull_mode} != 1 ] && [ ${code_pull_mode} != 2 ]; then
              echo "Error: the value of code update mode is incorrect."
              exit 1;
            fi
            shift ;;
        -d|--debug)
            llvm_debug_mode="$2"
            if [ ${llvm_debug_mode} != 0 ] && [ ${llvm_debug_mode} != 1 ] && [ ${llvm_debug_mode} != 2 ]; then
              echo "Error: the value of llvm debug mode is incorrect."
              exit 1;
            fi
            shift;;
        -n|--number)
            number_compile_task="$2"
            shift;;
        -b|--build)
            build_project="$2"
            shift;;
        -l|--linx)
            COMPILER_INFO="$2"
            shift;;
        -h|--help)
            help_info
            exit 0 ;;
        *)
            echo "${BUILD_SCRIPT} unknown option: $1"
            help_info
            exit 1 ;;
      esac
      shift
    done
  echo "------------------------------Options for building-------------------------------"
  echo "LLVM version: ${llvm_version}"
  echo "Code update mode: ${code_pull_mode}"
  echo "LLVM debug mode: ${llvm_debug_mode}"
  echo "Number of parallel compilation: ${number_compile_task}"
  echo "Project to build: ${build_project}"
  echo "Version number of Linx server toochain: ${COMPILER_INFO}"
  echo
}

update_code() {
echo "Getting linx server llvm toolchain source code"
pushd $SRC_PATH

while [ $# != 0 ]; do
  if [ ! -d $1 ]; then
    git clone ${GIT_PATH[$1]}
  elif [ ${code_pull_mode} -eq 1 ]; then
    pushd $1 && git pull && popd
  elif [ ${code_pull_mode} -eq 2 ]; then
    rm -rf $SRC_PATH/$1
    git clone ${GIT_PATH[$1]}
  fi
  shift
done

popd
}

get_llvm_project() {
  update_code "${LLVM12_GIT_NAME}"
  update_code "${BINUTILS_GIT_NAME}"
}

get_server_code() {
  get_llvm_project
}

set_llvm_debug_mode() {
  if [ ${llvm_debug_mode} -eq 0 ]; then
    LLVM_BUILD_TYPE="Release"
    LLVM_ASSERTIONS="OFF"
  elif [ ${llvm_debug_mode} -eq 2 ]; then
    LLVM_BUILD_TYPE="Debug"
  fi
}

create_new_file(){
while [ $# != 0 ] ; do
[ -n "$1" ] && rm -rf $1 ; mkdir -p $1; shift; done }

create_nonexistent_file(){
  while [ $# != 0 ] ; do
    if [ ! -d "$1" ]; then
      mkdir -p $1;
    fi
    shift;
  done
}

build_binutils() {
  echo "Building binutils..."
  create_new_file $BUILD_OBJ/build-binutils $PREFIX/bin/linx64
  pushd $BUILD_OBJ/build-binutils
  $BINUTILS_SRC_PATH/configure  --target="linx64-unknown-elf" --prefix="${PREFIX}" --with-pkgversion="${COMPILER_INFO}" --with-sysroot="${SYSROOT}" --enable-plugins --enable-ld=yes --with-system-zlib --disable-libssp --disable-werror --disable-multilib
  make -j "${number_compile_task}" && make install prefix=$PREFIX exec_prefix=$PREFIX libdir=$PREFIX/lib64
  mv $PREFIX/bin/linx64-unknown-elf* $PREFIX/bin/linx64
  popd
}

build_llvm() {
  echo "Building llvm..."
  create_new_file $BUILD_OBJ/build-llvm
  pushd $BUILD_OBJ/build-llvm
  cmake -G Ninja $LLVM_SRC_PATH/llvm/ -DCMAKE_BUILD_TYPE="${LLVM_BUILD_TYPE}" -DLLVM_ENABLE_ASSERTIONS="${LLVM_ASSERTIONS}" -DCMAKE_INSTALL_PREFIX="${PREFIX}"  -DLLVM_TARGETS_TO_BUILD="${LLVM_TARGETS_TO_BUILD}" -DLLVM_ENABLE_PROJECTS="${LLVM_ENABLE_PROJECTS}" -DCLANG_REPOSITORY_STRING="${COMPILER_INFO}" -DLLVM_LINK_LLVM_DYLIB="${LLVM_LINK_LLVM_DYLIB}" -DLLVM_ENABLE_CLASSIC_FLANG=on
  ninja -j "${number_compile_task}" && ninja install
  popd
}

#build_glibc(){
#  echo "Building glibc..."
#  readonly HOST="$(gcc -dumpmachine)"
#  if [[ $HOST == *x86_64* ]]
#  then
#      echo "The HOST: $HOST machine is x86_64"
#      create_nonexistent_file $PREFIX $PREFIX/riscv64-linux-gnu/lib
#      cp -rf $PWD/add_I_L/x86_64/sysroot $PREFIX
#      cp -rf $PWD/add_I_L/x86_64/lib64 $PREFIX
#      cp $PWD/add_I_L/x86_64/riscv64-linux-gnu/lib/lib* $PREFIX/riscv64-linux-gnu/lib
#      cp -rf $PWD/add_I_L/x86_64/riscv64-linux-gnu/include $PREFIX/riscv64-linux-gnu
#      cp -rf $PWD/add_I_L/x86_64/riscv64-linux-gnu/sys-include $PREFIX/riscv64-linux-gnu
#      pushd $PREFIX/sysroot/usr/lib/libc
#      ar rcs $PREFIX/sysroot/usr/lib/libc.a $PREFIX/sysroot/usr/lib/libc/*
#      popd
#  elif [[ $HOST == *aarch64* ]]
#  then
#      echo "The HOST: $HOST machine is aarch64"
#      create_nonexistent_file $PREFIX $PREFIX/riscv64-linux-gnu/lib
#      cp -rf $PWD/add_I_L/aarch64/sysroot $PREFIX
#      cp -rf $PWD/add_I_L/aarch64/lib64 $PREFIX
#      cp $PWD/add_I_L/aarch64/riscv64-linux-gnu/lib/lib* $PREFIX/riscv64-linux-gnu/lib
#      cp -rf $PWD/add_I_L/aarch64/riscv64-linux-gnu/include $PREFIX/riscv64-linux-gnu
#      cp -rf $PWD/add_I_L/aarch64/riscv64-linux-gnu/sys-include $PREFIX/riscv64-linux-gnu
#      pushd $PREFIX/sysroot/usr/lib/libc
#      ar rcs $PREFIX/sysroot/usr/lib/libc.a $PREFIX/sysroot/usr/lib/libc/*
#      popd
#  else
#      echo "Error: NO glibc type for the $HOST machine"
#  fi
#}

build_linx_toolchain() {
  if [ ${build_project} == "all" ]; then
    create_new_file $OUTPUT $INSTALL
    build_binutils
    build_llvm
    #build_glibc
    # copy some library and header files from gcc, because llvm can't build the libc now.
    cp -nr ./linx64-unknown-elf/linx64-unknown-elf $INSTALL/
    cp -nr ./linx64-unknown-elf/lib64 $INSTALL/
  else
    build_project_temp="${build_project}-"
    while [ "${build_project_temp}" ]; do
      project_name=${build_project_temp%%-*}
      if [[ ${build_project_name} != *${project_name}* ]]; then
         echo "Error: No the project ${project_name}"
         exit 1;
      fi
      "build_${project_name}"
      build_project_temp="${build_project_temp#*-}"
    done
  fi
}
pack_linx_toolchain() {
  if [ ${build_project} == "all" ]; then
    # delete the dir of share
    [ -n "$PREFIX/share" ] && rm -rf $PREFIX/share

    echo "Packing linx block isa toolchain"
    tar --format=gnu -czf $OUTPUT/$INSTALL.tar.gz $INSTALL
  fi
}

########################################build linx toolchain######################################
# get options
get_opt "$@"

# set debug mode for llvm
set_llvm_debug_mode

# get code for llvm-project10.0/llvm-project12.0 binutils
get_server_code

# build specified project for linx server toolchain
build_linx_toolchain

pack_linx_toolchain


echo "Build Complete!"
