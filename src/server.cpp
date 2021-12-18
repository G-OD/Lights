#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <vector>
#include <thread>
#include <chrono>
#include <shared_mutex>
#include <algorithm>
#include <sstream>

#include "Client.hpp"
#include "Light.hpp"

int sock;
struct sockaddr_in serverAddr;
std::vector<Client> clients;
auto lastChargeAll = std::chrono::high_resolution_clock::now();

std::shared_mutex clientMutex;
std::shared_mutex lightMutex;

void simulate();
void update();

int main() {
	Light::lights.emplace_back(-1, 250, 250, 50, 50, true, 1000, 1000, true, 20, 20);

	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);

	std::cout << "Starting Server..." << std::endl;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	fcntl(sock, F_SETFD, O_NONBLOCK);
	{
		int opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(1337);

	bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
	listen(sock, 5);

	// Main loop
	std::thread simulateThread(simulate);
	simulateThread.detach();
	std::thread updateThread(update);
	updateThread.detach();

	// Listen for clients
	std::cout << "Listening for connections..." << std::endl;
	while (1) {
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(sock, &read_fds);

		struct timeval tv = {0};
		tv.tv_usec = 66000;

		select(sock + 1, &read_fds, NULL, NULL, &tv);
		if (FD_ISSET(sock, &read_fds)) {
			char buf[100] = {0};
			recvfrom(sock, buf, 100, 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

			// Check if client has already connected
			bool found = false;
			clientMutex.lock_shared();
			for (auto &client: clients) {
				if (client.same(clientAddr)) {
					found = true;

					clientMutex.unlock_shared();
					clientMutex.lock();
					client.updateLastMsg();
					clientMutex.unlock();
					clientMutex.lock_shared();

					std::string data = buf;

					if (data[0] == 'T' && data.length() >= 2) {
						int id = std::atoi(data.substr(1).c_str());

						lightMutex.lock();
						for (auto &light: Light::lights) {
							if (light.GetID() == id) {
								light.toggle();
								break;
							}
						}
						lightMutex.unlock();
					}
					else if (data[0] == 'M' && std::count(data.begin(), data.end(), ',') == 2) {
						int id = std::atoi(data.substr(1).c_str());

						std::vector<std::string> results;
						std::stringstream ss(data);
						std::string item;
						while (std::getline(ss, item, ',')) {
							results.push_back(item);
						}

						float x = std::atof(results[1].c_str());
						float y = std::atof(results[2].c_str());

						lightMutex.lock();
						for (auto &light: Light::lights) {
							if (light.GetID() == id) {
								light.SetX(x);
								light.SetY(y);
								break;
							}
						}
						lightMutex.unlock();
					}
					else if (data[0] == 'C' && std::count(data.begin(), data.end(), ',') == 4) {
						data.erase(0, 1);

						std::vector<std::string> results;
						std::stringstream ss(data);
						std::string item;
						while (std::getline(ss, item, ',')) {
							results.push_back(item);
						}

						float x = std::atof(results[0].c_str());
						float y = std::atof(results[1].c_str());
						int maxBattery = std::atoi(results[2].c_str());
						bool flicker = (results[3] == "1") ? true : false;
						int flickerInterval = std::atoi(results[4].c_str());

						lightMutex.lock();
						Light::lights.push_back(Light(-1, x, y, 50, 50, false, maxBattery, maxBattery, flicker, flickerInterval, flickerInterval));
						lightMutex.unlock();
					}
					else if (data.substr(0, 2) == "DA") {
						lastChargeAll = std::chrono::high_resolution_clock::now();

						lightMutex.lock();
						Light::lights.clear();
						lightMutex.unlock();

						client.sendTo(sock, "DA", 2);
					}
					else if (data[0] == 'D' && data.length() >= 2) {
						int id = std::atoi(data.substr(1).c_str());

						lightMutex.lock();
						for (auto light = Light::lights.begin(); light != Light::lights.end(); light++) {
							if (light->GetID() == id) {
								Light::lights.erase(light);

								for (auto &c: clients) {
									std::string data = "D";
									data += std::to_string(id);
									c.sendTo(sock, data.c_str(), data.length());
								}

								break;
							}
						}
						lightMutex.unlock();
					}
					else if (data.substr(0, 2) == "PA") {
						lastChargeAll = std::chrono::high_resolution_clock::now();

						lightMutex.lock();
						Light::SetChargeAll(true);
						lightMutex.unlock();
					}
					else if (data[0] == 'P' && data.length() >= 2) {
						int id = std::atoi(data.substr(1).c_str());

						lightMutex.lock();
						for (auto &light: Light::lights) {
							if (light.GetID() == id) {
								light.SetCharge(true);
								break;
							}
						}
						lightMutex.unlock();
					}

					break;
				}
			}
			clientMutex.unlock_shared();

			if (!found) {
				clientMutex.lock();
				clients.push_back(Client(clientAddr));
				clientMutex.unlock();
			}
		}
	}
}

void simulate() {
	while (1) {
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastChargeAll).count() >= 300) {
			Light::SetChargeAll(false);
		}

		lightMutex.lock();
		for (auto &light: Light::lights)
			light.update();
		lightMutex.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

void update() {
	while (1) {
		std::cout << "Client Count: " << clients.size() << std::endl;

		clientMutex.lock_shared();
		for (auto client = clients.begin(); client != clients.end(); ) {
			if (client->disconnected()) {
				clientMutex.unlock_shared();
				clientMutex.lock();
				clients.erase(client);
				clientMutex.unlock();
				clientMutex.lock_shared();
				continue;
			}

			lightMutex.lock_shared();
			for (auto &light: Light::lights) {
				std::string data = "";

				data += std::to_string(light.GetID());
				data += ",";
				data += std::to_string((int)light.GetX());
				data += ",";
				data += std::to_string((int)light.GetY());
				data += ",";
				data += std::to_string((int)light.GetWidth());
				data += ",";
				data += std::to_string((int)light.GetHeight());
				data += ",";
				data += std::to_string(light.GetPower());
				data += ",";
				data += std::to_string(light.GetOn());
				data += ",";
				data += std::to_string(light.GetMaxBattery());
				data += ",";
				data += std::to_string(light.GetBattery());
				data += ",";
				data += std::to_string(light.GetFlicker());
				data += ",";
				data += std::to_string(light.GetFlickerInterval());
				data += ",";
				data += std::to_string(light.GetFlickerTimer());

				std::cout << data << std::endl;
				client->sendTo(sock, data.c_str(), data.length());
			}

			if (Light::lights.size() == 0)
				client->sendTo(sock, "0", 1);

			lightMutex.unlock_shared();
			client++;
		}
		clientMutex.unlock_shared();

		std::this_thread::sleep_for(std::chrono::milliseconds(66));
	}
}