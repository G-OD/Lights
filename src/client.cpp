#include <iostream>

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#else
#include <GL/freeglut.h>
#include <GL/gl.h>
#endif

#if (_WIN32__ || _WIN64)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#endif

#include <thread>
#include <sstream>
#include <algorithm>
#include <shared_mutex>

#include "Light.hpp"

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500
#define FPS 60


int sock;
auto lastMsg = std::chrono::high_resolution_clock::now();
float mouseX = 0;
float mouseY = 0;
std::shared_mutex lightMutex;
std::shared_mutex lastMsgMutex;
int maxBattery = 1000;
bool charge = false;
bool chargeAll = false;
bool flickering = false;
int flickerInterval = 20;

void reshape(int width, int height);
void display();
void timer(int state);
void mouse(int button, int state, int x, int y);
void mouseMotion(int x, int y);
void keyboard(unsigned char key, int x, int y);
void keyboardUp(unsigned char key, int x, int y);
void syncNetwork();

int main(int argc, char **argv) {
	struct sockaddr_in serverAddr;

	#if (_WIN32__ || _WIN64)
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	#endif

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	#if (_WIN32__ || _WIN64)
	u_long mode = 1;  // 1 to enable non-blocking socket
	ioctlsocket(sock, FIONBIO, &mode);
	#else
	fcntl(sock, F_SETFD, O_NONBLOCK);
	#endif

	serverAddr.sin_family = AF_INET;
	if (argc >= 2)
		serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	serverAddr.sin_port = htons(1337);

	connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);;
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Light Simulator - 2019");

	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutTimerFunc(1000.0f / FPS, timer, 0);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(mouseMotion);
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);

	std::thread t(syncNetwork);
	t.detach();

	glutMainLoop();
}

void reshape(int width, int height) {
	if (height == 0) height = 1;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 1);
}

void display() {
	glClear(GL_COLOR_BUFFER_BIT);

	lightMutex.lock_shared();
	for (auto &light: Light::lights)
		light.render();
	lightMutex.unlock_shared();

	glutSwapBuffers();
}

void timer(int state) {
	send(sock, "0", 1, 0);

	std::cout << "Charge All: " << chargeAll << std::endl;
	std::cout << "Max Battery: " << maxBattery << std::endl;
	std::cout << "Flickering: " << flickering << std::endl;
	std::cout << "Flicker Interval: " << flickerInterval << std::endl;


	if (chargeAll)
		send(sock, "PA", 2, 0);

	lightMutex.lock_shared();
	if (Light::GetSelected() != nullptr) {
		Light::GetSelected()->move(mouseX, mouseY);

		std::string data = "M";
		data += std::to_string(Light::GetSelected()->GetID());
		data += ",";
		data += std::to_string(Light::GetSelected()->GetX());
		data += ",";
		data += std::to_string(Light::GetSelected()->GetY());

		send(sock, data.c_str(), data.length(), 0);
	}

	lastMsgMutex.lock_shared();
	for (auto light = Light::lights.begin(); light != Light::lights.end(); ) {
		std::cout << "Battery: " << light->GetBattery() << std::endl;

		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() < 1) {
			if (light->deleted()) {
				lightMutex.unlock_shared();
				lightMutex.lock();
				Light::lights.erase(light);
				lightMutex.unlock();
				lightMutex.lock_shared();
				continue;
			}
		}

		lightMutex.unlock_shared();
		lightMutex.lock();
		if (chargeAll || (charge && light->collide(mouseX, mouseY))) {
			if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() >= 1) {
				light->Charge();
			}

			std::string data = "P";
			data += std::to_string(light->GetID());
			send(sock, data.c_str(), data.length(), 0);
		}
		light->update();
		lightMutex.unlock();
		lightMutex.lock_shared();

		light++;
	}
	lastMsgMutex.unlock_shared();
	lightMutex.unlock_shared();

	std::cout << std::endl;

	glutPostRedisplay();
	glutTimerFunc(1000.0f / FPS, timer, 0);
}


void mouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		lightMutex.lock_shared();
		lastMsgMutex.lock_shared();
		for (auto &light: Light::lights) {
			if (light.collide(x, y)) {
				if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() >= 1) {
					lightMutex.unlock_shared();
					lightMutex.lock();
					light.toggle();
					lightMutex.unlock();
					lightMutex.lock_shared();
				}

				std::string data = "T";
				data += std::to_string(light.GetID());
				send(sock, data.c_str(), data.length(), 0);
				break;
			}
		}
		lastMsgMutex.unlock_shared();
		lightMutex.unlock_shared();
	}
	else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
		lightMutex.lock_shared();
		Light::select(x, y);
		lightMutex.unlock_shared();
	}
	else if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN) {
		lightMutex.lock_shared();
		Light::deselect();

		bool found = false;
		for (auto light = Light::lights.begin(); light != Light::lights.end(); light++) {
			if (light->collide(x, y)) {
				found = true;

				lastMsgMutex.lock_shared();
				if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() >= 1) {
					lightMutex.unlock_shared();
					lightMutex.lock();
					Light::lights.erase(light);
					lightMutex.unlock();
					lightMutex.lock_shared();
				}
				lastMsgMutex.unlock_shared();

				std::string data = "D";
				data += std::to_string(light->GetID());

				send(sock, data.c_str(), data.length(), 0);
				break;
			}
		}
		lightMutex.unlock_shared();

		if (!found) {
			lastMsgMutex.lock_shared();
			if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() >= 1) {
				lightMutex.lock();
				Light::lights.emplace_back(-1, x, y, 50, 50, false, maxBattery, maxBattery, flickering, flickerInterval, flickerInterval);
				lightMutex.unlock();
			}
			lastMsgMutex.unlock_shared();

			std::string data = "C";
			data += std::to_string(x);
			data += ",";
			data += std::to_string(y);
			data += ",";
			data += std::to_string(maxBattery);
			data += ",";
			data += std::to_string(flickering);
			data += ",";
			data += std::to_string(flickerInterval);

			send(sock, data.c_str(), data.length(), 0);
		}
	}
}

void mouseMotion(int x, int y) {
	mouseX = x;
	mouseY = y;
}

void syncNetwork() {
	// Receive data from server
	while (1) {
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(sock, &read_fds);

		select(sock + 1, &read_fds, NULL, NULL, NULL);

		if (FD_ISSET(sock, &read_fds)) {
			char buf[100] = {0};
			if (recv(sock, buf, 100, 0) > 0) {
				lastMsgMutex.lock();
				lastMsg = std::chrono::high_resolution_clock::now();
				lastMsgMutex.unlock();

				std::string data = buf;

				if (std::count(data.begin(), data.end(), ',') == 11) {
					std::vector<std::string> results;
					std::stringstream ss(data);
					std::string item;
					while (std::getline(ss, item, ',')) {
						results.push_back(item);
					}

					int id = std::atoi(results[0].c_str());
					float x = std::atof(results[1].c_str());
					float y = std::atof(results[2].c_str());
					float width = std::atof(results[3].c_str());
					float height = std::atof(results[4].c_str());
					bool power = (results[5] == "1") ? true : false;
					bool on = (results[6] == "1") ? true : false;
					int maxBattery = std::atoi(results[7].c_str());
					int battery = std::atoi(results[8].c_str());
					bool flicker = (results[9] == "1") ? true : false;
					int flickerInterval = std::atoi(results[10].c_str());
					int flickerTimer = std::atoi(results[11].c_str());

					bool found = false;
					lightMutex.lock_shared();
					for (auto &light: Light::lights) {
						if (light.GetID() == id) {
							found = true;
							lightMutex.unlock_shared();
							lightMutex.lock();
							light.updateLastMsg();

							if (Light::GetSelected() == nullptr) {
								light.SetX(x);
								light.SetY(y);
							} else if (Light::GetSelected()->GetID() != id) {
								light.SetX(x);
								light.SetY(y);
							}
							light.SetWidth(width);
							light.SetHeight(height);
							light.SetPower(power);
							light.SetOn(on);
							light.SetMaxBattery(maxBattery);
							light.SetBattery(battery);
							light.SetFlicker(flicker);
							light.SetFlickerTimer(flickerTimer);

							lightMutex.unlock();
							lightMutex.lock_shared();

							break;
						}
					}
					lightMutex.unlock_shared();

					if (!found) {
						lightMutex.lock();
						Light::lights.emplace_back(id, x, y, width, height, power, maxBattery, battery, flicker, flickerInterval, flickerTimer);
						lightMutex.unlock();
					}
				}
				else if (data.substr(0, 2) == "DA") {
					lightMutex.lock();
					Light::lights.clear();
					lightMutex.unlock();
				}
				else if (data[0] == 'D' && data.length() >= 2) {
					int id = std::atoi(data.substr(1).c_str());

					lightMutex.lock();
					for (auto light = Light::lights.begin(); light != Light::lights.end(); light++) {
						if (light->GetID() == id) {
							Light::lights.erase(light);
							break;
						}
					}
					lightMutex.unlock();
				}
			}
		}
	}
}

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
		case 'd':
			send(sock, "DA", 2, 0);
			break;
		case ' ':
			charge = true;
			break;
		case 'f':
			flickering = !flickering;
			break;
		case 'c':
			chargeAll = !chargeAll;
			break;
		case '-':
			if (maxBattery > 100)
				maxBattery -= 100;
			break;
		case '=':
			maxBattery += 100;
			break;
		case '_':
			if (flickerInterval > 10)
				flickerInterval -= 10;
			break;
		case '+':
			flickerInterval += 10;
			break;
	}
}

void keyboardUp(unsigned char key, int x, int y) {
	switch (key) {
		case ' ':
			charge = false;
			break;
	}
}