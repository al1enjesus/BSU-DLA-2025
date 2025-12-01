#!/bin/bash
echo "Building supervisor..."
gcc -Wall -Wextra -std=c99 -o supervisor supervisor.c config.c

echo "Building worker..." 
gcc -Wall -Wextra -std=c99 -o worker worker.c config.c

echo "Build complete!"
ls -la supervisor worker 2>/dev/null || echo "Some binaries might not have been created"
