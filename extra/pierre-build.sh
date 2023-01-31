#!/bin/env /bin/zsh

/usr/bin/cmake \
  --no-warn-unused-cli \
	-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
  -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
  -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
  -S/home/thughey/devel/pierre \
  -B/home/thughey/devel/pierre/build \
  -G Ninja

/usr/bin/cmake \
  --build /home/thughey/devel/pierre/build \
  --config RelWithDebugInfo \
  --target all --
