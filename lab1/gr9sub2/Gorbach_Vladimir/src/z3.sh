#!/bin/bash
grep -oP '\s+install\s+\K\S+' /var/log/dpkg.log \
	| sort \
	| uniq -c \
	| sort -nr \
	| head -n 10
