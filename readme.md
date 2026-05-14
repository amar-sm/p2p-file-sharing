# P2P File Sharing System

A multi-peer torrent-style file sharing system built using C++, TCP sockets, and multithreading.

This project supports:

- Chunk-based downloading
- Parallel downloads using threads
- Tracker-based peer discovery
- Automatic dead-peer cleanup
- Heartbeat/PING mechanism
- Same-laptop and multi-laptop support
- Colorful command-line UI


# Features

## Multi-Peer Downloading

Files are divided into chunks and downloaded simultaneously from multiple peers.

## Tracker-Based Architecture

A central tracker server maintains:
- file-to-peer mappings
- active peers
- peer availability

## Automatic Dead-Peer Cleanup

Peers periodically send heartbeat messages.

Inactive peers are automatically removed from the tracker.

## Parallel Chunk Downloads

Each chunk is downloaded in a separate thread.

Maximum 5 parallel threads are used at once.

## Same-Laptop + Multi-Laptop Support

Supports:
- localhost testing (`127.0.0.1`)
- LAN/WiFi-based peer sharing
  

# Technologies Used
- C++
- POSIX TCP Sockets
- Multithreading (`std::thread`)
- Linux Networking
- File I/O

## How To Run

### 1. Compile Tracker

```bash
g++ tracker.cpp -pthread -o tracker
```

---

### 2. Run Tracker

```bash
./tracker
```

Leave tracker terminal running.

---

### 3. Compile Peer

```bash
g++ peer.cpp -pthread -o peer
```

---

### 4. Run Peer

```bash
./peer
```

Peer will ask:

```txt
Enter port:
Enter TRACKER IP:
Enter YOUR laptop IP:
```

---

## SAME LAPTOP TESTING

### Peer 1

Enter:

```txt
Port: 5000
TRACKER IP: 127.0.0.1
YOUR laptop IP: 127.0.0.1
```

Create test file:

```bash
echo "Hello World" > notes.txt
```

Register file:

```txt
register notes.txt
```

---

### Peer 2

Run another peer:

```bash
./peer
```

Enter:

```txt
Port: 6000
TRACKER IP: 127.0.0.1
YOUR laptop IP: 127.0.0.1
```

Download file:

```txt
download notes.txt
```

Downloaded file:

```txt
downloaded_notes.txt
```

---

## MULTI-LAPTOP TESTING

Connect all laptops to same WiFi/LAN.

---

### Find Tracker Laptop IP

Run on tracker laptop:

```bash
hostname -I
```

Example:

```txt
192.168.1.5
```

---

### Tracker Laptop

Run:

```bash
./tracker
```

---

### Peer Laptop A

Run:

```bash
./peer
```

Example:

```txt
Port: 5000
TRACKER IP: 192.168.1.5
YOUR laptop IP: 192.168.1.9
```

Create test file:

```bash
echo "Network Test File" > movie.txt
```

Register file:

```txt
register movie.txt
```

---

### Peer Laptop B

Run:

```bash
./peer
```

Example:

```txt
Port: 6000
TRACKER IP: 192.168.1.5
YOUR laptop IP: 192.168.1.12
```

Download file:

```txt
download movie.txt
```

---

## Useful Commands

```txt
register <file>
download <file>
unregister <file>
delete <file>
trackerlist
list
exit
```

## Project Structure

```txt
tracker.cpp   -> Tracker server
peer.cpp      -> Peer client + file server
