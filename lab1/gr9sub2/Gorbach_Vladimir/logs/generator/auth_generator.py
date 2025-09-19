# Generating a large synthetic /var/log/auth.log sample for testing
# File will be written to /mnt/data/auth.log.big
import random
from datetime import datetime, timedelta

random.seed(42)

OUT = "./../auth.test"
lines = []

# time window: Sep 10-15, 2025
start = datetime(2025, 9, 10, 0, 0)
num_lines = 5000

hosts = ["server", "web01", "db1", "gateway"]
users = ["root", "admin", "alice", "bob", "john", "guest", "oracle", "support", "testuser", "invalid"]
ips_common = ["203.0.113.5", "203.0.113.7", "203.0.113.9",
              "198.51.100.25", "185.34.56.78", "192.0.2.10",
              "192.0.2.45", "192.168.1.50", "10.0.0.12", "10.10.10.10"]
ips_private = ["127.0.0.1", "::1"]
ipv6_examples = ["2001:db8::1", "2001:0db8:85a3::8a2e:0370:7334"]

# message templates
templates = [
    # sshd failed with ip
    ("sshd[{pid}]: Failed password for {when_user} from {ip} port {port} ssh2", 0.5),
    ("sshd[{pid}]: Invalid user {user} from {ip} port {port}", 0.15),
    ("sshd[{pid}]: Failed password for invalid user {user} from {ip} port {port} ssh2", 0.05),
    ("sshd[{pid}]: Failed password for {when_user} from unknown port 0 ssh2", 0.02),
    ("sshd[{pid}]: Failed password for {when_user} from {ip} port {port} ssh2", 0.03),
    # accepted logins
    ("sshd[{pid}]: Accepted password for {when_user} from {ip} port {port} ssh2", 0.02),
    # local tty/no ip
    ("sshd[{pid}]: Invalid user {user} from tty1", 0.02),
    ("su: pam_unix(su:auth): authentication failure; logname={logname} uid={uid} euid={euid} tty=/dev/tty1 ruser= rhost=", 0.03),
    ("sudo:     {user} : TTY=pts/{pts} ; PWD=/home/{user} ; USER=root ; COMMAND=/bin/false", 0.03),
    ("sudo: pam_unix(sudo:auth): authentication failure; logname={user} uid={uid} euid=0 tty=/dev/pts/{pts} ruser= rhost=", 0.03),
    # ipv6 failed
    ("sshd[{pid}]: Failed password for invalid user {user} from {ipv6} port {port} ssh2", 0.02)
]

# normalize weights
weights = [w for (_, w) in templates]
total = sum(weights)
weights = [w/total for w in weights]

for i in range(num_lines):
    # random timestamp
    delta = timedelta(seconds=random.randint(0, int((datetime(2025,9,15)-start).total_seconds())))
    ts = start + delta
    tstr = ts.strftime("%b %d %H:%M:%S")
    host = random.choice(hosts)
    # pick template
    tmpl = random.choices([t for (t,_) in templates], weights=weights, k=1)[0]
    pid = random.randint(800, 4000)
    user = random.choice(users)
    when_user = random.choice(["root", "invalid user "+user, user])
    ip = random.choice(ips_common + ips_private)
    ipv6 = random.choice(ipv6_examples)
    port = random.randint(1024, 65000)
    pts = random.randint(0,9)
    uid = random.randint(0, 2000)
    euid = 0 if random.random() < 0.2 else uid
    logline = tmpl.format(pid=pid, user=user, when_user=when_user, ip=ip, ipv6=ipv6, port=port, pts=pts, uid=uid, euid=euid, logname=user)
    lines.append(f"{tstr} {host} {logline}")

# shuffle a bit to avoid strictly chronological file
random.shuffle(lines)

# ensure we have many "Failed" & "Invalid" to test
# add bursts from specific attacking IPs
attackers = ["203.0.113.5", "198.51.100.25", "185.34.56.78", "192.168.1.50", "203.0.113.9"]
for attacker in attackers:
    for j in range(80):
        ts = datetime(2025,9,14, random.randint(0,23), random.randint(0,59), random.randint(0,59))
        tstr = ts.strftime("%b %d %H:%M:%S")
        pid = random.randint(1000, 5000)
        port = random.randint(1024, 65000)
        lines.append(f"{tstr} server sshd[{pid}]: Failed password for root from {attacker} port {port} ssh2")

# write file
with open(OUT, "w") as f:
    for line in lines:
        f.write(line + "\n")

# provide a small preview and summary
total_lines = len(lines)
failed_count = sum(1 for l in lines if "Failed" in l or "Invalid" in l)
unique_ips = set()
import re
for l in lines:
    for m in re.findall(r'([0-9]{1,3}\.){3}[0-9]{1,3}', l):
        # m is only last group from regex due to grouping; better extract full matches differently
        pass
# better ip extract
ip_matches = re.findall(r'(?:[0-9]{1,3}\.){3}[0-9]{1,3}', "\n".join(lines))
unique_ips = set(ip_matches)

print("Wrote file:", OUT)
print("Total log lines:", total_lines)
print("Lines with Failed/Invalid:", failed_count)
print("Unique IPv4 addresses found:", len(unique_ips))
print()
print("First 20 lines preview:")
with open(OUT) as f:
    for _ in range(20):
        print(f.readline().rstrip())

