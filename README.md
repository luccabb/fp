# fp

Find free ports. Uses the kernel's own port allocator (`bind` to port 0) so the returned port is guaranteed free at the time of query.

## Install

```
brew install luccabb/tap/fp
```

Or build from source:

```
make && sudo make install
```

## Usage

```
fp              # print a free TCP port
fp -n 5         # print 5 unique free ports
fp -r 8000:9000 # free port in range
fp -u           # UDP instead of TCP
fp -6           # IPv6
fp -n 3 -r 8000:8100 -u  # combine flags
```

## How it works

`fp` creates a socket, binds it to port 0, and reads back the port the kernel assigned. This is the most reliable method possible - no parsing `netstat`, no reading `/proc`, no guessing. The OS kernel itself guarantees the port is available.

For range-constrained searches, `fp` try-binds each port with a randomised start offset to minimise collisions between concurrent invocations.

## License

MIT
