#!/bin/sh

if [ "$#" -lt 1 ]; then
    echo 'Directory not specified' >&2
    exit 1
fi

if [ -d "$1/.git" ]; then
    echo 'Updating AUTHORS'

    { echo '# Generated — do not edit.' &&
      echo &&
      names=$(git -C $1 log --no-merges --pretty=format:"%an" src 2>/dev/null) &&
      echo "${names}" | sort | uniq; } > $1/AUTHORS.tmp &&
    mv -f $1/AUTHORS.tmp $1/AUTHORS ||
    { rm -f $1/AUTHORS.tmp &&
      echo 'Failed to generate AUTHORS' >&2; }
else
    echo 'Git repository not found' >&2
    exit 1
fi
