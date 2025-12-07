timofey@timofey-Victus-by-HP-Laptop-16-e0xxx:~$ mkdir -p /tmp/source
mkdir -p /mnt/fuse
mkdir: cannot create directory ‘/mnt/fuse’: Permission denied
timofey@timofey-Victus-by-HP-Laptop-16-e0xxx:~$ sudo su
[sudo] password for timofey:
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# mkdir -p /tmp/source
mkdir -p /mnt/fuse
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# rm -rf /tmp/source/*
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# sudo chown $USER:$USER /mnt/fuse
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# sudo mkdir -p /mnt/fuse
sudo chown $USER:$USER /mnt/fuse
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# ls -ld /mnt/fuse
drwxr-xr-x 2 root root 4096 сне  7 19:29 /mnt/fuse
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "Hello passthrough" > /mnt/fuse/test.txt
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /tmp/source/test.txt
Hello passthrough
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /mnt/fuse/test.txt
Hello passthrough
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "Modified" >> /mnt/fuse/test.txt
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /tmp/source/test.txt
Hello passthrough
Modified
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# rm /mnt/fuse/test.txt
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# ls /tmp/source
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "Hello Linus" > /mnt/fuse/up.txt
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /tmp/source/up.txt
Hello Linus
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /mnt/fuse/up.txt
HELLO LINUS
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "Abc123! Привет Linus" > /tmp/source/mix.txt
cat /mnt/fuse/mix.txt
ABC123! Привет LINUS
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "testWrite" > /mnt/fuse/w.txt
cat /tmp/source/w.txt
testWrite
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /mnt/fuse/w.txt
TESTWRITE
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# fusermount3 -u /mnt/fuse
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "Hello Linus" > /mnt/fuse/rot.txt
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /tmp/source/rot.txt
Uryyb Yvahf
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /mnt/fuse/rot.txt
Hello Linus
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# Hello Linus
Command 'Hello' not found, did you mean:
command 'hello' from snap hello (2.10)
command 'hello' from deb hello (2.10-2ubuntu4)
command 'hello' from deb hello-traditional (2.10-5)
command 'jello' from deb jello (1.5.2-1)
See 'snap info <snapname>' for additional versions.
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /mnt/fuse/rot.txt
Hello Linus
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# echo "Abc123! Привет Linus" > /mnt/fuse/mix.txt
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cat /tmp/source/mix.txt
Nop123! Привет Yvahf
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# head -c 20 /dev/urandom > /mnt/fuse/bin.dat
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# cmp /mnt/fuse/bin.dat /tmp/source/bin.dat
/mnt/fuse/bin.dat /tmp/source/bin.dat differ: byte 1, line 1
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey# fusermount3 -u /mnt/fuse
root@timofey-Victus-by-HP-Laptop-16-e0xxx:/home/timofey#