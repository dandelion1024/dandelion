#!/bin/sh

cd $(dirname $0)
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=YES .
