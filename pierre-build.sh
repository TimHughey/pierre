#!/usr/bin/env zsh

base=${HOME}/devel/pierre

pushd -q $base

mkdir -p build/beta

pushd -q build/beta

action=$argv[1]

if [[ $# -gt 0 ]]
then
  shift;
fi

case ${action} in

  configure)
    # cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=debug -GNinja ${base}
    cmake --clean-first -S ${base} -GNinja -B build \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release
    ;;

  build)
    cmake --build build -- -v
    ;;

  run)
    ninja -v && ./pierre $@
    ;;

  clean)
    cmake --clean-first -S ${base} -GNinja -B build \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -- -v && ./pierre $@
    ;;

  *)
    ninja -v
    ;;
esac

popd -q +2
