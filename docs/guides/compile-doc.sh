#!/bin/sh

cd `dirname ${0}`

if [ $1 = 'doc-html' ]; then
	make html
elif [ $1 = 'clean' ]; then
	make clean
fi
