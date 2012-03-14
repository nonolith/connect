#!/bin/bash

if [ -d .git ]
then
	VERSION="$(git describe --always --dirty='*')"
	echo "Building git version $VERSION"
	echo "const char* server_version=\"$VERSION\";" > version.cpp
elif [ -f version.cpp ]
then
	echo "Using version file"
else
	echo "No version.cpp and not a git checkout!"
	exit 1
fi
