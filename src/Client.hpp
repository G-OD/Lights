#ifndef CLIENT_H
#define CLIENT_H

#include <chrono>

class Client {
	struct sockaddr_in addr;
	socklen_t addrLen;
	std::chrono::high_resolution_clock::time_point lastMsg;

public:
	Client(struct sockaddr_in addr) {
		this->addr = addr;
		this->addrLen = sizeof(addr);
		this->lastMsg = std::chrono::high_resolution_clock::now();
	}

	void updateLastMsg() {
		lastMsg = std::chrono::high_resolution_clock::now();
	}

	bool disconnected() {
		return (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() >= 1) ? true : false;
	}

	bool same(struct sockaddr_in addr) {
		return (this->addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
				this->addr.sin_port == addr.sin_port) ? true : false;
	}

	ssize_t recvFrom(int sock, char *buf, int len) {
		return recvfrom(sock, buf, len, 0, (struct sockaddr *)&addr, &addrLen);
	}

	void sendTo(int sock, const char *msg, int len) {
		sendto(sock, msg, len, 0, (struct sockaddr *)&addr, addrLen);
	}
};

#endif