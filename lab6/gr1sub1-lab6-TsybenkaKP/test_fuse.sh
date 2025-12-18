#!/bin/bash
set -e

MOUNT=/tmp/mount
SRC=/tmp/source

echo "Running basic tests against $MOUNT (assumes FS is mounted)"
echo "Creating files..."
echo "Hello FUSE" > $MOUNT/test.txt
cat $MOUNT/test.txt

echo "Appending..."
echo "Line2" >> $MOUNT/test.txt
echo "Content:"
cat $MOUNT/test.txt

echo "Creating dir and nested file..."
mkdir -p $MOUNT/sub
echo "Nested" > $MOUNT/sub/n.txt
ls -la $MOUNT
cat $MOUNT/sub/n.txt

echo "Removing files..."
rm $MOUNT/test.txt
rmdir $MOUNT/sub
echo "OK"

