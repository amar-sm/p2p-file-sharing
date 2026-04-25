## BitTorrent-like P2P File Sharing System (C++)

A multi-peer, chunk-based file sharing system implemented in C++ using socket programming and multithreading.
This project simulates core features of BitTorrent, including parallel downloads, peer discovery, and real-time progress tracking.

## Features
> Tracker-based peer discovery
> Chunk-based file transfer
> Parallel downloads using multithreading
> Multi-peer load distribution
> Real-time progress bar with speed & ETA
> Retry mechanism for failed chunks
> Thread-safe file writing using mutex
> Colored terminal UI


## System Architecture
> Tracker maintains file to peer mapping
> Peers act as both client & server
> Files are split into chunks and downloaded in parallel

## Requirements
> Linux/Ubuntu/WSL
> g++ compiler
> POSIX sockets

## Compilation
g++ tracker.cpp -o tracker
g++ peer.cpp -o peer -pthread

## File Setup

Make sure the file exists in the project directory:

```bash
cd ~/p2p
dd if=/dev/urandom of=file1 bs=1K count=20
```

## How to Run
1. Start Tracker

```bash
./tracker
```

2. Start Peers(in separate terminals)

```bash
./peer
Enter port: 9001
> register file1
```

```bash
./peer
Enter port: 9002
> register file1
```

3. Download File

```bash
./peer
Enter port: 9003
> download file1
```

## Key Concepts Used
> Socket Programming (TCP)
> Multithreading (std::thread)
> Synchronization (mutex)
> File I/O (binary mode)
> Load balancing
> Basic distributed systems design


## Author

Amarpreet Samra


## Conclusion
This project demonstrates core distributed system concepts and is suitable for networking.

