#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <mount_point> <fs_name>"
    exit 1
fi

MOUNT_POINT=$1
FS_NAME=$2
TEST_DIR="${MOUNT_POINT}/iops_test"
RESULTS_FILE="iops_${FS_NAME}.csv"

echo "File Count,Create Time (s),Delete Time (s),Create IOPS,Delete IOPS" > $RESULTS_FILE

# Test with different numbers of files
for count in 100 500 1000; do
    echo "Testing with $count files..."
    
    mkdir -p $TEST_DIR
    
    # Create files
    start_time=$(date +%s.%N)
    for i in $(seq 1 $count); do
        dd if=/dev/zero of=${TEST_DIR}/file_$i bs=1K count=1 status=none
    done
    create_time=$(echo "$(date +%s.%N) - $start_time" | bc)
    
    # Delete files
    start_time=$(date +%s.%N)
    rm -rf $TEST_DIR
    delete_time=$(echo "$(date +%s.%N) - $start_time" | bc)
    
    mkdir -p $TEST_DIR
    
    create_iops=$(echo "scale=2; $count / $create_time" | bc)
    delete_iops=$(echo "scale=2; $count / $delete_time" | bc)
    
    echo "${count},${create_time},${delete_time},${create_iops},${delete_iops}" >> $RESULTS_FILE
    
    rm -rf $TEST_DIR
done

echo "Results saved to $RESULTS_FILE"
