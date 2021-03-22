#!/usr/bin/env zsh

base=${HOME}/devel/pierre

pushd -q $base

build_dir=${base}/build/${BUILD:=wip}

mkdir -p ${build_dir}

action=$argv[1]

if [[ $# -gt 0 ]]
then
  shift;
fi

case ${action} in

  configure)
    # cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=debug -GNinja ${base}
    cmake --clean-first -S ${base} -GNinja -B ${build_dir} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release
    ;;

  build)
    cmake -S ${base} -GNinja -B ${build_dir} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release \
    && cmake --build ${build_dir} -- -v
    ;;

  ninja)
    pushd -q ${build_dir}
    ninja -v
    popd -q
    ;;

  clean)
    cmake --clean-first -S ${base} -GNinja -B ${build_dir} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . -- -v && ./pierre $@
    ;;

  *)
     cmake --build ${build_dir} && ${build_dir}/pierre $@
    ;;

esac
