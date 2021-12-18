#ifndef LIGHT_H
#define LIGHT_H

#include <vector>

class Light {
	#ifndef SERVER
	static Light *selected;
	std::chrono::high_resolution_clock::time_point lastMsg;
	#endif

	static int nextID;
	static bool chargeAll;

	int id;
	float x;
	float y;
	float width;
	float height;
	bool power;
	bool on = true;
	bool charge = false;
	std::chrono::high_resolution_clock::time_point lastCharge = std::chrono::high_resolution_clock::now();
	int maxBattery;
	int battery;
	bool flicker;
	int flickerInterval;
	int flickerTimer;

public:
	static std::vector<Light> lights;

	Light(int id, float x, float y, float width, float height, bool power, int maxBattery, int battery, bool flicker, int flickerInterval, int flickerTimer) {
		if (id == -1) {
			this->id = nextID;
			nextID++;
		} else
			this->id = id;

		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;
		this->power = power;

		this->maxBattery = maxBattery;
		this->battery = battery;

		this->flicker = flicker;
		this->flickerInterval = flickerInterval;
		this->flickerTimer = flickerTimer;

		#ifndef SERVER
		updateLastMsg();
		#endif
	}

	#ifndef SERVER
	static Light* const GetSelected() {
		return selected;
	}

	static constexpr void deselect() {
		selected = nullptr;
	}

	static void select(float x, float y) {
		if (selected == nullptr) {
			for (auto &light: lights) {
				if (light.collide(x, y)) {
					selected = &light;
					break;
				}
			}
		} else
			selected = nullptr;
	}

	void updateLastMsg() {
		lastMsg = std::chrono::high_resolution_clock::now();
	}

	bool deleted() {
		return (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - lastMsg).count() >= 1) ? true : false;
	}

	void render() {
		if (power && on && battery > 0) {
			glBegin(GL_POLYGON);
			glColor3f(1, 1, 1);
			glVertex2f(x - (width / 2), y - (height / 2));
			glVertex2f(x - (width / 2), y + (height / 2));
			glVertex2f(x + (width / 2), y + (height / 2));
			glVertex2f(x + (width / 2), y - (height / 2));
			glEnd();
		}

		glBegin(GL_LINE_LOOP);
		glColor3f(1, 1, 0);
		glVertex2f(x - (width / 2), y - (height / 2));
		glVertex2f(x - (width / 2), y + (height / 2));
		glVertex2f(x + (width / 2), y + (height / 2));
		glVertex2f(x + (width / 2), y - (height / 2));
		glEnd();
	}
	#endif

	constexpr int GetID() const {
		return id;
	}
	constexpr float GetX() const {
		return x;
	}
	constexpr float GetY() const {
		return y;
	}
	constexpr float GetWidth() const {
		return width;
	}
	constexpr float GetHeight() const {
		return height;
	}
	constexpr bool GetPower() const {
		return power;
	}
	constexpr bool GetOn() const {
		return on;
	}
	constexpr int GetMaxBattery() const {
		return maxBattery;
	}
	constexpr int GetBattery() const {
		return battery;
	}
	constexpr int GetFlicker() const {
		return flicker;
	}
	constexpr int GetFlickerInterval() const {
		return flickerInterval;
	}
	constexpr int GetFlickerTimer() const {
		return flickerTimer;
	}

	constexpr void SetX(float x) {
		this->x = x;
	}
	constexpr void SetY(float y) {
		this->y = y;
	}
	constexpr void SetWidth(float width) {
		this->width = width;
	}
	constexpr void SetHeight(float height) {
		this->height = height;
	}
	constexpr void SetPower(bool power) {
		this->power = power;
	}
	constexpr void SetOn(bool on) {
		this->on = on;
	}
	constexpr void SetMaxBattery(int maxBattery) {
		this->maxBattery = maxBattery;
	}
	constexpr void SetBattery(int battery) {
		this->battery = battery;
	}
	void SetCharge(bool charge) {
		lastCharge = std::chrono::high_resolution_clock::now();
		this->charge = charge;
	}
	static constexpr void SetChargeAll(bool charge) {
		chargeAll = charge;
	}
	constexpr void SetFlicker(bool flicker) {
		this->flicker = flicker;
	}
	constexpr void SetFlickerTimer(float timer) {
		flickerTimer = timer;
	}

	void update() {
		if (flicker && power && battery > 0) {
			if (flickerTimer <= 0) {
				flickerTimer = flickerInterval;
				on = !on;
			}
			flickerTimer--;
		}

		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastCharge).count() >= 300)
			SetCharge(false);
		if (chargeAll || charge)
			Charge();

		if (power && on && battery > 0)
			battery--;
		else if (battery < 0)
			battery = 0;
	}

	constexpr void Charge() {
		if (battery < maxBattery)
			battery += 2;
		if (battery > maxBattery)
			battery = maxBattery;
	}

	constexpr void toggle() {
		power = !power;
	}

	constexpr bool collide(float x, float y) {
		return (x >= this->x - (width / 2) &&
				x <= this->x + (width / 2) &&
				y >= this->y - (height / 2) &&
				y <= this->y + (height / 2)) ? true : false;
	}

	constexpr void move(float x, float y) {
		this->x = x;
		this->y = y;
	}
};


#ifndef SERVER
Light *Light::selected = nullptr;
#endif

std::vector<Light> Light::lights;
int Light::nextID = 0;
bool Light::chargeAll = false;

#endif