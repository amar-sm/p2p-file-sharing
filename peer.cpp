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
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

string TRACKER_IP;
string MY_IP;

mutex fileMutex;
vector<string> sharedFiles;

string formatTime(double seconds) {

    if (seconds < 60)
        return to_string(seconds) + "s";

    int m = seconds / 60;
    int s = (int)seconds % 60;

    return to_string(m) + "m " + to_string(s) + "s";
}

int connectTo(string ip, int port) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv;

    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);

    inet_pton(AF_INET, ip.c_str(), &serv.sin_addr);

    int status =
        connect(sock, (sockaddr*)&serv, sizeof(serv));

    if (status < 0) {

        close(sock);

        return -1;
    }

    return sock;
}

void heartbeatLoop(int port) {

    while (true) {

        this_thread::sleep_for(chrono::seconds(5));

        for (auto &file : sharedFiles) {

            int sock = connectTo(TRACKER_IP, TRACKER_PORT);

            if (sock < 0)
                continue;

            string msg =
                "PING " +
                file + " " +
                MY_IP + " " +
                to_string(port);

            send(sock, msg.c_str(), msg.size(), 0);

            close(sock);
        }
    }
}

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

void registerFile(string filename, int port) {

    int sock = connectTo(TRACKER_IP, TRACKER_PORT);

    if (sock < 0) {

        cout << RED << "Tracker offline\n" << RESET;

        return;
    }

    string msg =
        "REGISTER " +
        filename + " " +
        MY_IP + " " +
        to_string(port);

    send(sock, msg.c_str(), msg.size(), 0);

    close(sock);
}

void unregisterFile(string filename, int port) {

    int sock = connectTo(TRACKER_IP, TRACKER_PORT);

    if (sock < 0)
        return;

    string msg =
        "UNREGISTER " +
        filename + " " +
        MY_IP + " " +
        to_string(port);

    send(sock, msg.c_str(), msg.size(), 0);

    close(sock);
}

void deleteFileFromTracker(string filename) {

    int sock = connectTo(TRACKER_IP, TRACKER_PORT);

    if (sock < 0)
        return;

    string msg = "DELETE " + filename;

    send(sock, msg.c_str(), msg.size(), 0);

    close(sock);
}

void showTrackerFiles() {

    int sock = connectTo(TRACKER_IP, TRACKER_PORT);

    if (sock < 0)
        return;

    string msg = "LIST";

    send(sock, msg.c_str(), msg.size(), 0);

    char buffer[4096] = {0};

    read(sock, buffer, 4096);

    cout << CYAN << buffer << RESET;

    close(sock);
}

vector<pair<string,int>> getPeers(string filename) {

    int sock = connectTo(TRACKER_IP, TRACKER_PORT);

    if (sock < 0)
        return {};

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

void downloadChunk(string ip, int port, string filename, int chunk, fstream &out) {

    cout << CYAN << "\nConnecting to peer " << ip << ":" << port << RESET << endl;

    int sock = connectTo(ip, port);

    if (sock < 0) {

        cout << RED << "Peer " << ip << ":" << port << " offline" << RESET << endl;

        return;
    }

    string req = filename + " " + to_string(chunk);

    send(sock, req.c_str(), req.size(), 0);

    char buffer[CHUNK_SIZE];

    int bytes = read(sock, buffer, CHUNK_SIZE);

    if (bytes > 0) {

        lock_guard<mutex> lock(fileMutex);

        out.seekp(chunk * CHUNK_SIZE);

        out.write(buffer, bytes);

        cout << GREEN << "Chunk " << chunk << " received from " << ip << ":" << port << RESET << endl;
    }

    close(sock);
}

void downloadFile(string filename) {

    auto peers = getPeers(filename);

    if (peers.empty()) {

        cout << RED << "No peer found\n" << RESET;

        return;
    }

    cout << GREEN << "\nDownloading: " << filename << RESET << endl;

    int sock = connectTo(peers[0].first,
                         peers[0].second);

    if (sock < 0) {

        cout << RED << "First peer offline\n" << RESET;

        return;
    }

    string req = filename + " 0";

    send(sock, req.c_str(), req.size(), 0);

    int size;

    if (read(sock, &size, sizeof(size)) <= 0 ||
        size == -1) {

        cout << RED << "File not found\n" << RESET;

        close(sock);

        return;
    }

    close(sock);

    int chunks =
        (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    cout << CYAN << "Peers: " << peers.size() << " | Chunks: " << chunks << RESET << endl;

    string outName = "downloaded_" + filename;

    fstream out(outName,
                ios::in |
                ios::out |
                ios::binary);

    if (!out) {

        out.open(outName,
                 ios::out |
                 ios::binary);

        out.close();

        out.open(outName,
                 ios::in |
                 ios::out |
                 ios::binary);
    }

    vector<bool> done(chunks, false);

    int completed = 0;

    auto start = chrono::high_resolution_clock::now();

    while (completed < chunks) {

        vector<thread> batch;

        for (int i = 0;
             i < chunks && batch.size() < 5;
             i++) {

            if (done[i])
                continue;

            auto p = peers[i % peers.size()];

            batch.emplace_back([&, i, p]() {

                downloadChunk(p.first,
                              p.second,
                              filename,
                              i,
                              out);

                lock_guard<mutex> lock(fileMutex);

                done[i] = true;
            });
        }

        for (auto &t : batch)
            t.join();

        completed = 0;

        for (bool x : done)
            if (x)
                completed++;

        auto now =
            chrono::high_resolution_clock::now();

        double seconds =
            chrono::duration<double>(
            now - start).count();

        double speed =
            (completed * CHUNK_SIZE / 1024.0)
            / max(seconds, 0.001);

        double remaining = chunks - completed;

        double eta =
            (remaining * CHUNK_SIZE / 1024.0)
            / max(speed, 0.001);

        int width = 30;

        float ratio =
            (float)completed / chunks;

        int filled = ratio * width;

        cout << "\r" << CYAN << "[";

        for (int i = 0; i < width; i++) {

            if (i < filled)
                cout << "#";
            else
                cout << "-";
        }

        cout << "] ";

        cout << GREEN
             << (int)(ratio * 100)
             << "% ";

        cout << YELLOW
             << "Speed: "
             << speed
             << " KB/s ";

        cout << "ETA: "
             << formatTime(eta)
             << " ";

        cout << "Done: "
             << completed
             << "/"
             << chunks;

        cout << RESET << flush;
    }

    out.close();

    cout << "\n"
         << GREEN
         << "Download complete: "
         << outName
         << RESET << endl;
}

int main() {

    int port;

    cout << "Enter port: ";
    cin >> port;
    cin.ignore();

    cout << "Enter TRACKER IP: ";
    getline(cin, TRACKER_IP);

    cout << "Enter YOUR laptop IP: ";
    getline(cin, MY_IP);

    thread server([&]() {
        serveFile(port);
    });

    server.detach();

    thread heartbeat([&]() {
        heartbeatLoop(port);
    });

    heartbeat.detach();

    cout << BOLD << CYAN;
    cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    cout << "         MULTI-PEER P2P CLIENT      \n";
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    cout << RESET;

    while (true) {

        cout << BLUE
             << "\nCommands:\n"
             << RESET;

        cout << GREEN
             << "register <file>\n"
             << RESET;

        cout << CYAN
             << "download <file>\n"
             << RESET;

        cout << YELLOW
             << "unregister <file>\n"
             << RESET;

        cout << MAGENTA
             << "delete <file>\n"
             << RESET;

        cout << BLUE
             << "trackerlist\n"
             << RESET;

        cout << CYAN
             << "list\n"
             << RESET;

        cout << RED
             << "exit\n> "
             << RESET;

        string line;

        getline(cin, line);

        stringstream ss(line);

        string cmd, file;

        ss >> cmd >> file;

        if (cmd == "register") {

            registerFile(file, port);

            sharedFiles.push_back(file);

            cout << GREEN
                 << "Registered\n"
                 << RESET;
        }

        else if (cmd == "download") {
            downloadFile(file);
        }

        else if (cmd == "list") {
            system("ls");
        }

        else if (cmd == "unregister") {

            unregisterFile(file, port);

            cout << YELLOW
                 << "Unregistered\n"
                 << RESET;
        }

        else if (cmd == "delete") {

            deleteFileFromTracker(file);

            cout << RED
                 << "Deleted from tracker\n"
                 << RESET;
        }

        else if (cmd == "trackerlist") {
            showTrackerFiles();
        }

        else if (cmd == "exit") {

            for (auto &f : sharedFiles) {

                unregisterFile(f, port);

                cout << YELLOW
                     << "Unregistered: "
                     << f
                     << RESET << endl;
            }

            cout << RED
                 << "Closing peer...\n"
                 << RESET;

            exit(0);
        }

        else {

            cout << RED
                 << "Invalid command\n"
                 << RESET;
        }
    }

    return 0;
}