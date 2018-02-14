## Assignment 2: Go-back-N

### Network Setup

Useful commands:

```
$ sudo iptables -L ## List all existing rules in the filter table
$ sudo iptables -F ## Flush all existing rules in the filter table
$ sudo iptables -t nat -L ## List all existing rules in the nat table
$ sudo iptables -t nat -F ## Flush all existing rules in the nat table
```

Simulate unreliable network:

```
$ sudo iptables -A INPUT -p udp -s vm-b -d vm-a -j NFQUEUE --queue-num 0
$ sudo iptables -A OUTPUT -p udp -s vm-a -d vm-b -j NFQUEUE --queue-num 0
$ sudo troller <drop_ratio> <reorder_ratio>
```

Internet connection in `vm-b` and `vm-c`:

```
vm-a$ sudo iptables -A FORWARD -i eth0 -j ACCEPT
vm-a$ sudo iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE
```

```
vm-b$ sudo route add default gw vm-a
vm-c$ sudo route add default gw vm-a
```

### Deploying to the VMs

```
$ git remote add vm ssh://csci4430@projgw.cse.cuhk.edu.hk:13053/home/csci4430/csci4430/.git
$ git push vm --force
vm-a$ git reset --hard
```

Deploy from `vm-a` to `vm-b`

```
vm-a$ git push vm-b --force
vm-b$ git reset --hard
```

TODO:
Share the directories across VMs using NFS... this is kinda stupid.
Well, still better than `scp` anyway.

### Build & Run

```
$ make
$ ./myfypserver <port number>
$ ./myftpclient <server ip addr> <server port> <file> <N> <timeout>
```
