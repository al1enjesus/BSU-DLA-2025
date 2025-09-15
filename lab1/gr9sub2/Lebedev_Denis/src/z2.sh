#!/bin/bash
cat /var/log/auth.log \
	| grep -iE 'Failed|Invalid' \
	| grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' \
	| sort \
	| uniq -c \
	| sort -nr \
	| head -n 10 \
	| sed -E 's/([0-9]+\.[0-9]+\.[0-9]+)\.[0-9]+/\1.x/'
