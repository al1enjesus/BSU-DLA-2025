make
sudo dmesg -C
sudo insmod syscall_spy.ko
./benchmark > benchmark.log
cat benchmark.log
sudo rmmod syscall_spy
dmesg > syscall_spy.log