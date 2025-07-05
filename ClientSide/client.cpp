#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<ctime>
#include<windows.h>
#include<string>
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


void recvmsg(SOCKET soc) {
	char store[8192];
	int  len;

	while ((len = recv(soc, store, sizeof(store), 0)) > 0) {
		// 1) Audible alert
		cout << '\a';

		// 2) Read the raw incoming string
		string raw(store, len);
		auto   pos = raw.find(" : ");

		// 3) Clear your current input line:
		//    \r moves cursor to start of this line,
		//    \x1b[K erases from cursor to end of line.
		cout << "\r\x1b[K";

		// 4) Print the incoming message
		if (pos != string::npos) {
			// Name in green + rest of message
			cout
				<< "\x1b[32m" << raw.substr(0, pos) << "\x1b[0m"
				<< raw.substr(pos)
				<< '\n';
		}
		else {
			// Fallback if no " : " found
			cout << raw << '\n';
		}

		// 5) Re‑print your prompt on a clean new line
		cout << "\x1b[32myou\x1b[0m: " << flush;
	}

	// Connection closed notice
	cout << "\n\033[31m[Disconnected]\033[0m\n";
	closesocket(soc);
	WSACleanup();
}





void sendmsg(SOCKET soc) {
	cout << "Enter your ChatName: " << endl;
	string s;
	getline(cin, s);
	string msg;

	while (true) {
		// prompt “you:” in green
		cout << "\x1b[32myou\x1b[0m: ";
		getline(cin, msg);
		if (msg == "quit") {
			cout << "Application Closed. See you soon" << endl;
		}

		// timestamp logic (unchanged)
		std::time_t now = std::time(nullptr);
		std::tm      t;
#if defined(_MSC_VER)
		localtime_s(&t, &now);
#else
		t = *std::localtime(&now);
#endif
		char buf[20];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);

		msg = s
			+ " : "
			+ msg
			+ " \x1b[90m["   // start dim‑gray text
			+ buf
			+ "]\x1b[0m";

		int sz = send(soc,
			msg.c_str(),
			static_cast<int>(msg.length()),
			0);
		if (sz == SOCKET_ERROR) {
			cout << "Error Sending Message" << endl;
			break;
		}
	}

	closesocket(soc);
	WSACleanup();
}


bool initialize() {
	WSADATA data;
	return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}



int main() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD  dwMode;
	GetConsoleMode(hOut, &dwMode);
	SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	if (!initialize()) {
		cout << "initialisation of winsock library failed" << endl;
		return 0;
	}

	SOCKET soc;
	soc = socket(AF_INET, SOCK_STREAM, 0);
	if (soc == INVALID_SOCKET) {
		cout << "invalid socket" << endl;
		return 1;
	}

	int port = 12345;
	sockaddr_in serveradd;
	string serveraddr = "127.0.0.1";
	serveradd.sin_family = AF_INET;
	serveradd.sin_port = htons(port);
	inet_pton(AF_INET, serveraddr.c_str(), &(serveradd.sin_addr));

	//now connecting
	if (connect(soc, reinterpret_cast<sockaddr*>(&serveradd), sizeof(serveradd)) == SOCKET_ERROR) {
		cout << "some problm connecting with server" << endl;
		closesocket(soc);
		WSACleanup();
		return 1;
	}

	//connected 
	cout << "connected successfully" << endl;
	cout << "\x1b[90mType /help for commands (quit, /whisper, /users)\x1b[0m\n";

	thread sendthread(sendmsg, soc);
	thread recthread(recvmsg, soc);



	sendthread.join();
	recthread.join();
	//cout << "Here" << endl;
	/*closesocket(soc);
	WSACleanup();*/
	return 0;
}