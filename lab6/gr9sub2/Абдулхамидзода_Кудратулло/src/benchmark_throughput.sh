#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <mount_point> <fs_name>"
    exit 1
fi

MOUNT_POINT=$1
FS_NAME=$2
RESULTS_FILE="throughput_${FS_NAME}.csv"

echo "File Size,Write Speed (MB/s),Read Speed (MB/s)" > $RESULTS_FILE

# Test different file sizes
for size in 1 10 100; do
    echo "Testing ${size}MB file..."
    
    # Write test
    write_speed=$(dd if=/dev/zero of=${MOUNT_POINT}/test_${size}MB bs=1M count=$size 2>&1 | \
                 grep -o '[0-9.]\+ MB/s' | head -1 | cut -d' ' -f1)
    
    # Read test  
    read_speed=$(dd if=${MOUNT_POINT}/test_${size}MB of=/dev/null bs=1M 2>&1 | \
                grep -o '[0-9.]\+ MB/s' | head -1 | cut -d' ' -f1)
    
    echo "${size},${write_speed},${read_speed}" >> $RESULTS_FILE
    
    # Cleanup
    rm -f ${MOUNT_POINT}/test_${size}MB
done

echo "Results saved to $RESULTS_FILE"
