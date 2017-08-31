#!/bin/sh

if [ "$#" -ge 2 ] && [ -d "$1/.git" ]; then
	{
		echo '# Generated — do not edit.';
		echo;
		git -C $1 log --no-merges --pretty=format:"%an" src | sort | uniq
	} > $2
fi
