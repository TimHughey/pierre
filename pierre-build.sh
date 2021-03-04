#!/usr/bin/env zsh

pushd -q .

mkdir -p build

pushd build

cmake -GNinja .. && ninja
