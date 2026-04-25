#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <sstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include <mutex>
#include <chrono>

using namespace std;

#define TRACKER_PORT 8080
#define CHUNK_SIZE 1024

#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"

string formatTime(double seconds) {
    if (seconds < 60) return to_string(seconds) + "s";
    int m = seconds / 60;
    int s = (int)seconds % 60;
    return to_string(m) + "m " + to_string(s) + "s";
}

mutex fileMutex;

//Socket Helper

int connectTo(string ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv.sin_addr);

    connect(sock, (sockaddr*)&serv, sizeof(serv));
    return sock;
}

//Server

void serveFile(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    while (true) {
        int client = accept(server_fd, NULL, NULL);

        char req[256] = {0};
        read(client, req, 256);

        string filename;
        int chunk;

        stringstream ss(req);
        ss >> filename >> chunk;

        ifstream file(filename, ios::binary);

        if (!file) {
            int err = -1;
            send(client, &err, sizeof(err), 0);
            close(client);
            continue;
        }

        file.seekg(0, ios::end);
        int size = file.tellg();

        if (chunk == 0) {
            send(client, &size, sizeof(size), 0);
            close(client);
            continue;
        }

        file.seekg(chunk * CHUNK_SIZE);

        char buffer[CHUNK_SIZE];
        file.read(buffer, CHUNK_SIZE);

        send(client, buffer, file.gcount(), 0);

        file.close();
        close(client);
    }
}

//tracker

void registerFile(string filename, int port) {
    int sock = connectTo("127.0.0.1", TRACKER_PORT);

    string msg = "REGISTER " + filename + " 127.0.0.1 " + to_string(port);
    send(sock, msg.c_str(), msg.size(), 0);

    close(sock);
}

vector<pair<string,int>> getPeers(string filename) {
    int sock = connectTo("127.0.0.1", TRACKER_PORT);

    string msg = "GET " + filename;
    send(sock, msg.c_str(), msg.size(), 0);

    char buffer[1024] = {0};
    read(sock, buffer, 1024);

    close(sock);

    vector<pair<string,int>> peers;
    stringstream ss(buffer);
    string ip;
    int port;

    while (ss >> ip >> port) {
        peers.push_back({ip, port});
    }

    return peers;
}

//Download

void downloadChunk(string ip, int port, string filename, int chunk, fstream &out) {
    int sock = connectTo(ip, port);

    string req = filename + " " + to_string(chunk);
    send(sock, req.c_str(), req.size(), 0);

    char buffer[CHUNK_SIZE];
    int bytes = read(sock, buffer, CHUNK_SIZE);

    if (bytes > 0) {
        lock_guard<mutex> lock(fileMutex);
        out.seekp(chunk * CHUNK_SIZE);
        out.write(buffer, bytes);
    }

    close(sock);
}

//UI

void showProgress(int done, int total, double speed, double eta) {
    int width = 30;
    float ratio = (float)done / total;
    int filled = ratio * width;

    cout << "\r";

    cout << CYAN << "[";
    for (int i = 0; i < width; i++) {
        if (i < filled) cout << "#";
        else cout << "-";
    }
    cout << "] ";

    cout << GREEN << (int)(ratio * 100) << "% ";

    cout << YELLOW << "Speed: " << speed << " KB/s ";
    cout << "ETA: " << formatTime(eta) << " ";
    cout << "Done: " << done << "/" << total;

    cout << RESET << flush;
}

//Main Download

void downloadFile(string filename) {
    auto peers = getPeers(filename);

    if (peers.empty()) {
        cout << RED << "No peer found\n" << RESET;
        return;
    }

    cout << GREEN << "\nDownloading: " << filename << RESET << endl;

    int sock = connectTo(peers[0].first, peers[0].second);

    string req = filename + " 0";
    send(sock, req.c_str(), req.size(), 0);

    int size;
    if (read(sock, &size, sizeof(size)) <= 0 || size == -1) {
        cout << RED << "File not found\n" << RESET;
        close(sock);
        return;
    }

    close(sock);

    int chunks = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    cout << CYAN << "Peers: " << peers.size()
         << " | Chunks: " << chunks << RESET << endl;

    string outName = "downloaded_" + filename;
    fstream out(outName, ios::in | ios::out | ios::binary);

    if (!out) {
        out.open(outName, ios::out | ios::binary);
        out.close();
        out.open(outName, ios::in | ios::out | ios::binary);
    }

    vector<bool> done(chunks, false);
    int completed = 0;

    auto start = chrono::high_resolution_clock::now();

    while (completed < chunks) {
        vector<thread> batch;

        for (int i = 0; i < chunks && batch.size() < 5; i++) {
            if (done[i]) continue;

            auto p = peers[i % peers.size()];

            batch.emplace_back([&, i, p]() {
                downloadChunk(p.first, p.second, filename, i, out);

                lock_guard<mutex> lock(fileMutex);
                done[i] = true;
            });
        }

        for (auto &t : batch) t.join();

        completed = 0;
        for (bool x : done) if (x) completed++;

        auto now = chrono::high_resolution_clock::now();
        double seconds = chrono::duration<double>(now - start).count();

        double speed = (completed * CHUNK_SIZE / 1024.0) / max(seconds, 0.001);

        double remaining = (chunks - completed);
        double eta = (remaining * CHUNK_SIZE / 1024.0) / max(speed, 0.001);

        // ===== LIVE UI =====
        int width = 30;
        float ratio = (float)completed / chunks;
        int filled = ratio * width;

        cout << "\r" << CYAN << "[";
        for (int i = 0; i < width; i++) {
            if (i < filled) cout << "#";
            else cout << "-";
        }
        cout << "] ";

        cout << GREEN << (int)(ratio * 100) << "% ";
        cout << YELLOW << "Speed: " << speed << " KB/s ";
        cout << "ETA: " << formatTime(eta) << " ";
        cout << "Done: " << completed << "/" << chunks;

        cout << RESET << flush;
    }

    out.close();

    cout << "\n" << GREEN << "✔ Download complete: "
         << outName << RESET << endl;
}


int main() {
    int port;
    cout << "Enter port: ";
    cin >> port;
    cin.ignore();

    thread server([&]() { serveFile(port); });

    cout << GREEN << "\n=== P2P CLIENT ===\n" << RESET;

    while (true) {
        cout << "\nCommands:\n";
        cout << "register <file>\n";
        cout << "download <file>\n";
        cout << "list\n";
        cout << "exit\n> ";

        string line;
        getline(cin, line);

        stringstream ss(line);
        string cmd, file;
        ss >> cmd >> file;

        if (cmd == "register") {
            registerFile(file, port);
            cout << GREEN << "Registered\n" << RESET;
        }
        else if (cmd == "download") {
            downloadFile(file);
        }
        else if (cmd == "list") {
            system("ls");
        }
        else if (cmd == "exit") {
            exit(0);
        }
        else {
            cout << RED << "Invalid command\n" << RESET;
        }
    }

    server.join();
}