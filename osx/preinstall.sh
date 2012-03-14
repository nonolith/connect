#!/bin/bash
if [ "$(ps -A | grep nonolith-connect | grep -v grep)" != "" ]
	then killall nonolith-connect
fi
