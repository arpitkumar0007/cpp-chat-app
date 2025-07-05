#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<tchar.h>
#include<vector>
#include<windows.h>
#include<thread>

using namespace std;


#pragma comment(lib,"ws2_32.lib")

/*
===========================================================================
  CHAT APP WORKFLOW

  ┌────────────────────────────────────────────────────────────┐
  │                  SERVER SIDE (server.cpp)                │
  └────────────────────────────────────────────────────────────┘
	  │
	  │ 1) initialize Winsock
	  │
	  │ 2) create listen socket (TCP)
	  │
	  │ 3) bind() to 0.0.0.0:12345
	  │
	  │ 4) listen()
	  │
	  │ 5) loop:
	  │      ├─ accept() ➔ new client socket
	  │      ├─ spawn thread(interact, clientSocket, clients)
	  │      └─ store clientSocket in clients list
	  │
	  │ 6) in interact():
	  │      ├─ recv() first message → extract clientName
	  │      ├─ on each recv():
	  │      │     • log “Message sent by <clientName>”
	  │      │     • forward raw payload to all other clients
	  │      └─ on recv() ≤0:
	  │            • log “<clientName> disconnected”
	  │            • remove from clients list, closesocket()
	  │
	  │ 7) cleanup on shutdown
	  │
===========================================================================
  ┌────────────────────────────────────────────────────────────┐
  │                  CLIENT SIDE (client.cpp)                │
  └────────────────────────────────────────────────────────────┘
	  │
	  │ 1) SetConsoleCP/OutputCP → UTF‑8 (emoji support)
	  │
	  │ 2) Enable ANSI escapes via SetConsoleMode()
	  │
	  │ 3) initialize Winsock
	  │
	  │ 4) socket() → connect() to 127.0.0.1:12345
	  │
	  │ 5) on success:
	  │      ├─ std::thread(sendmsg, soc)
	  │      └─ std::thread(recvmsg, soc)
	  │
	  │ 6) sendmsg():
	  │      ├─ prompt “you:” in green
	  │      ├─ getline() user input
	  │      ├─ append timestamp [YYYY‑MM‑DD HH:MM:SS] (dim gray)
	  │      ├─ send(raw) via send()
	  │      └─ loop
	  │
	  │ 7) recvmsg():
	  │      ├─ recv() payload
	  │      ├─ beep (’\a’)
	  │      ├─ “\r\x1b[K” to clear partial prompt
	  │      ├─ print Name:msg (name in green)
	  │      ├─ newline + re‑print “you:” prompt
	  │      └─ loop
	  │
	  │ 8) on disconnect:
	  │      ├─ print “[Disconnected]” in red
	  │      └─ closesocket(), WSACleanup()
	  │
===========================================================================
*/


void interact(SOCKET clientsock, vector<SOCKET>& clients) {
	cout << "client connected" << endl;

	char  store[8192];
	bool  gotName = false;
	string clientName;

	while (true) {
		int sz = recv(clientsock, store, sizeof(store), 0);
		if (sz <= 0) {
			// client closed / errored out
			if (gotName) {
				cout << clientName << " disconnected" << endl;
			}
			else {
				cout << "UNKNOWN client disconnected" << endl;
			}
			break;
		}

		// pull their name out of "Name : message…"
		string raw(store, sz);
		auto   pos = raw.find(" : ");
		if (!gotName && pos != string::npos) {
			clientName = raw.substr(0, pos);
			gotName = true;
		}

		// notify server that a message arrived (we already know name)
		cout << "Message sent by "
			<< (gotName ? clientName : "<unknown>")
			<< endl;

		// forward to all others
		for (auto& client : clients) {
			if (client != clientsock) {
				send(client,
					raw.c_str(),
					static_cast<int>(raw.length()),
					0);
			}
		}
	}

	// cleanup
	auto it = find(clients.begin(), clients.end(), clientsock);
	if (it != clients.end()) clients.erase(it);
	closesocket(clientsock);
}


bool initialize() {
	WSADATA data;
	return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

int main() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	if (!initialize()) {
		cout << "initialisation of winsock library failed" << endl;
		return 1;
	}


	// creating listening socket
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		cout << "invalid socket created" << endl;
		return 1;
	}


	//adress structure
	int port = 12345;
	sockaddr_in serveradd;
	serveradd.sin_family = AF_INET;
	serveradd.sin_port = htons(port);

	if (InetPton(AF_INET, _T("0.0.0.0"), &serveradd.sin_addr) != 1) {
		cout << "wrong adress structure set" << endl;
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}


	//binding
	if (bind(listenSock, reinterpret_cast<sockaddr*>(&serveradd), sizeof(serveradd)) == SOCKET_ERROR) {
		cout << "binding error" << endl;
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}


	//listening
	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		cout << "listening falied" << endl;
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}



	//successfully connected
	cout << "server started at port: " << port << endl;


	//now accepting
	//vecttor to store the connected clients
	vector<SOCKET>clients;

	while (true) {
		SOCKET clientsock = accept(listenSock, nullptr, nullptr);
		if (clientsock == INVALID_SOCKET) {
			cout << "invalid client" << endl;
		}

		clients.push_back(clientsock);

		thread t1(interact, clientsock, std::ref(clients));
		t1.detach();

	}


	closesocket(listenSock);

	//cout << "here" << endl;




	WSACleanup();


	return 0;
}