#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

map<string, vector<string>> filePeers;

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    cout << "Tracker running...\n";

    while (true) {
        int addrlen = sizeof(address);
        int new_socket = accept(server_fd, (sockaddr*)&address, (socklen_t*)&addrlen);

        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);

        stringstream ss(buffer);
        string cmd, filename, ip;
        int port;

        ss >> cmd >> filename;

        if (cmd == "REGISTER") {
            ss >> ip >> port;

            filePeers[filename].push_back(ip + ":" + to_string(port));

            cout << "Registered: " << filename << " from "
                 << ip << ":" << port << endl;
        }
        else if (cmd == "GET") {
            string response;

            for (auto &p : filePeers[filename]) {
                string ip = p.substr(0, p.find(':'));
                string port = p.substr(p.find(':') + 1);

                response += ip + " " + port + " ";
            }

            send(new_socket, response.c_str(), response.size(), 0);

            cout << "Request for: " << filename << endl;
        }

        close(new_socket);
    }
}