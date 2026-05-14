#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <chrono>

using namespace std;

#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"
#define RESET "\033[0m"

struct PeerInfo {
    string peer;
    chrono::steady_clock::time_point lastSeen;
};

map<string, vector<PeerInfo>> filePeers;
mutex trackerMutex;

void removeDeadPeers() {

    while (true) {

        this_thread::sleep_for(chrono::seconds(10));

        lock_guard<mutex> lock(trackerMutex);

        auto now = chrono::steady_clock::now();

        for (auto it = filePeers.begin(); it != filePeers.end();) {

            auto &vec = it->second;

            vec.erase(remove_if(vec.begin(), vec.end(),
            [&](PeerInfo &p) {

                auto diff =
                    chrono::duration_cast<chrono::seconds>(
                    now - p.lastSeen).count();

                if (diff > 30) {

                    cout << RED
                         << "[AUTO REMOVED DEAD PEER] "
                         << p.peer
                         << " from "
                         << it->first
                         << RESET << endl;

                    return true;
                }

                return false;
            }), vec.end());

            if (vec.empty()) {
                it = filePeers.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

void removePeer(string filename, string peer) {

    if (filePeers.find(filename) == filePeers.end())
        return;

    auto &vec = filePeers[filename];

    vec.erase(remove_if(vec.begin(), vec.end(),
    [&](PeerInfo &p) {
        return p.peer == peer;
    }), vec.end());

    if (vec.empty()) {
        filePeers.erase(filename);
    }
}

int main() {

    thread cleanup(removeDeadPeers);
    cleanup.detach();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (sockaddr*)&address, sizeof(address));

    listen(server_fd, 10);

    cout << BOLD << CYAN;
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    cout << "         P2P TRACKER SERVER         \n";
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    cout << RESET;

    while (true) {

        int addrlen = sizeof(address);

        int new_socket =
            accept(server_fd,
                   (sockaddr*)&address,
                   (socklen_t*)&addrlen);

        char buffer[1024] = {0};

        read(new_socket, buffer, 1024);

        stringstream ss(buffer);

        string cmd;
        ss >> cmd;

        lock_guard<mutex> lock(trackerMutex);

        //REGISTER

        if (cmd == "REGISTER") {

            string filename, ip;
            int port;

            ss >> filename >> ip >> port;

            string peer = ip + ":" + to_string(port);

            bool exists = false;

            for (auto &p : filePeers[filename]) {

                if (p.peer == peer) {

                    p.lastSeen = chrono::steady_clock::now();
                    exists = true;
                    break;
                }
            }

            if (!exists) {

                PeerInfo p;

                p.peer = peer;
                p.lastSeen = chrono::steady_clock::now();

                filePeers[filename].push_back(p);

                cout << GREEN
                     << "[REGISTERED] "
                     << filename
                     << " from "
                     << peer
                     << RESET << endl;
            }
        }

        else if (cmd == "PING") {

            string filename, ip;
            int port;

            ss >> filename >> ip >> port;

            string peer = ip + ":" + to_string(port);

            for (auto &p : filePeers[filename]) {

                if (p.peer == peer) {

                    p.lastSeen = chrono::steady_clock::now();

                    break;
                }
            }
        }

        //GET

        else if (cmd == "GET") {

            string filename;
            ss >> filename;

            string response;

            if (filePeers.find(filename) != filePeers.end()) {

                for (auto &p : filePeers[filename]) {

                    string ip =
                        p.peer.substr(0, p.peer.find(':'));

                    string port =
                        p.peer.substr(p.peer.find(':') + 1);

                    response += ip + " " + port + " ";
                }
            }

            send(new_socket,
                 response.c_str(),
                 response.size(),
                 0);

            cout << CYAN
                 << "[GET] "
                 << filename
                 << RESET << endl;
        }

        //DELETE

        else if (cmd == "DELETE") {

            string filename;
            ss >> filename;

            if (filePeers.find(filename) != filePeers.end()) {

                filePeers.erase(filename);

                cout << RED
                     << "[DELETED FILE] "
                     << filename
                     << RESET << endl;
            }
        }

        //UNREGISTER

        else if (cmd == "UNREGISTER") {

            string filename, ip;
            int port;

            ss >> filename >> ip >> port;

            string peer = ip + ":" + to_string(port);

            removePeer(filename, peer);

            cout << YELLOW
                 << "[UNREGISTERED] "
                 << filename
                 << " from "
                 << peer
                 << RESET << endl;
        }

        //LIST

        else if (cmd == "LIST") {

            string response;

            response += "\n~~~~~~ TRACKER FILES ~~~~~~\n";

            for (auto &x : filePeers) {

                response += x.first + " -> ";

                for (auto &p : x.second) {
                    response += p.peer + " ";
                }

                response += "\n";
            }

            response += "~~~~~~~~~~~~~~~~~~~~~~~~~\n";

            send(new_socket,
                 response.c_str(),
                 response.size(),
                 0);

            cout << BLUE << response << RESET;
        }

        close(new_socket);
    }

    return 0;
}