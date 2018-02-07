#!/bin/bash

set -x

if [ ! -L "/usr/local/bin/clab" ]; then ln -s ./clab /usr/local/bin; fi
if [ ! -L "/usr/local/bin/alab" ]; then ln -s ./alab /usr/local/bin; fi
if [ ! -L "/usr/local/bin/hlab" ]; then ln -s ./hlab /usr/local/bin; fi
cd taggers
for file in hltag_*.py; do
	[ ! -L "/usr/local/bin/$file" ] || continue;
	ln -s $file /usr/local/bin/$file;
done
cd ..
