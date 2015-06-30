#!/bin/bash -i
# Indentation = 4 spaces.

if [ ! -d $1 ]; then
	cp -r $2 $1	
fi

mkdir -p $1/source/includes
cp $3/* $1/source/includes

cp $4 $1/source 

touch doc/slate.stamp
