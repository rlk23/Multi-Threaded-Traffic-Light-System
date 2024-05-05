#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <fstream>
#include <sstream>
#include <mutex>
#include <algorithm>
#include <map>
#include <condition_variable>
#include <chrono>
#include <random>
#include "Car.h"
#include <condition_variable>


using namespace std;

map<char, char> dict;


enum class LightState { RED, GREEN };

class TrafficLight {
public:
	LightState state;
	std::chrono::seconds defaultGreenDuration;
	std::chrono::seconds defaultRedDuration;
	std::chrono::seconds dynamicGreenDuration;

	
	TrafficLight()
		: state(LightState::RED),
		defaultGreenDuration(2),
		defaultRedDuration(2),
		dynamicGreenDuration(2) {}
		mutex mtx;
		condition_variable cv;
};

map<std::string, TrafficLight> trafficLights;


vector<Car> cars;

mutex mtx1;
condition_variable cv1;

ofstream outFile;

queue<int> northQ;
queue<int> southQ;
queue<int> eastQ;
queue<int> westQ;

mutex inter00;
mutex inter01;
mutex inter10;
mutex inter11;

mutex originN;
mutex originS;
mutex originE;
mutex originW;

mutex coutLock;

std::chrono::seconds calculateDynamicGreenDuration(const std::string& direction) {
	
	std::map<std::string, size_t> queueSizes = {
		{"north-south", northQ.size() + southQ.size()},
		{"east-west", eastQ.size() + westQ.size()}
	};

	
	size_t maxQueueSize = 0;
	for (const auto& pair : queueSizes) {
		if (pair.second > maxQueueSize) {
			maxQueueSize = pair.second;
		}
	}


	int baseDuration = 5;

	
	int extraDuration = std::min(5, static_cast<int>(maxQueueSize / 5)); 

	return std::chrono::seconds(baseDuration + extraDuration);
}

void trafficLightController() {
	while (true) {
		for (auto& pair : trafficLights) {
			const string& direction = pair.first;
			TrafficLight& light = pair.second;
			unique_lock<mutex> lk(light.mtx);

			
			light.dynamicGreenDuration = calculateDynamicGreenDuration(direction);

			
			cout << "Traffic Light for " << direction
				<< " is currently " << (light.state == LightState::GREEN ? "GREEN" : "RED")
				<< " [Current Duration: " << light.dynamicGreenDuration.count() << "s]" << endl;

		
			light.state = (light.state == LightState::GREEN) ? LightState::RED : LightState::GREEN;

		
			light.cv.notify_all();

		
			cout << "Traffic Light for " << direction
				<< " switching to " << (light.state == LightState::GREEN ? "GREEN" : "RED")
				<< " [Next Duration: " << light.dynamicGreenDuration.count() << "s]" << endl;

			lk.unlock();
			this_thread::sleep_for(light.dynamicGreenDuration);
		}
	}
}





int calculateDriveTime(const Car& c) {
	const double baseDistance = 100.0;
	int dynamicTime = static_cast<int>(baseDistance / c.speed);
	return dynamicTime;
}

bool compare(const Car& a, const Car& b) {

	return a.time < b.time;

}

void fileReader(string fileName) {

	ifstream input;
	input.open(fileName + ".txt");
	if (!input) {
		cerr << "Unable to open file " << fileName<< endl;
		exit(1);

	}
	string line;
	vector<Car> lines;
	int i = 1;

	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<> distr(60, 100);

	while (getline(input, line)) {
		stringstream linestream(line);
		string time;
		string dir;
		Car temp;

		getline(linestream, time, ' ');
		linestream >> dir;
		cout << dir << endl;
		if (dir.length() == 2) {
			temp.dest = dir[1];
			temp.origin = dir[0];
			temp.isTurning = true;
			cout << "origin: " << temp.origin << " destination: " << temp.dest << endl;

		}
		else {
			temp.dest = dir[0];
			temp.origin = dict[dir[0]];
			temp.isTurning = false;
			cout << "origin: " << temp.origin << " destination: " << temp.dest << endl;
		}
		temp.time = stoi(time);
		temp.id = i;
		cars.push_back(temp);
		temp.speed = distr(gen);
		i++;
	}
	input.close();

	sort(cars.begin(), cars.end(), compare);
	return;

}

void printLogs(Car c, int i) {

	switch (i) {
	case 1:
		cout << "CAR #" << c.id << " : Created" << endl;
		break;
	case 2:
		cout << "CAR #" << c.id << "  First in " << c.origin << " Queue" << endl;
		outFile << "CAR #" << c.id << "  First in " << c.origin << " Queue" << "\n";
		break;
	case 3:
		cout << "CAR #" << c.id << "  LEFT  " << "      Direction: " << c.origin << "-->" << c.dest << (c.isTurning ? ", Turning:" : ", Going Straight") << endl;
		outFile << "CAR #" << c.id << "  LEFT  " << "      Direction: " << c.origin << "-->" << c.dest << (c.isTurning ? ", Turning:" : ", Going Straight") << "\n";
		break;
	
	}
	return;
}



pair<queue<int>*, mutex*> getQueueAndMutex(const string& direction) {
	if (direction == "N") return { &northQ,&originN };
	else if (direction == "S") return { &southQ, &originS };
	else if (direction == "E") return { &eastQ, &originE };
	else if (direction == "W") return { &westQ, &originW };
	throw runtime_error("Invalid direction");

}

void processCar(Car c, string direction) {
	pair<queue<int>*, mutex*> queueAndMutex = getQueueAndMutex(direction);
	queue<int>* carQueue = queueAndMutex.first;
	mutex* originMutex = queueAndMutex.second;

	unique_lock<mutex> lk(coutLock);
	printLogs(c, 1);
	lk.unlock();

	this_thread::sleep_for(chrono::seconds(c.time));

	{
		lock_guard<mutex> lock(*originMutex);
		carQueue->push(c.id);

	}

	while (true) {
		lk.lock();
		if (!carQueue->empty() && carQueue->front() == c.id) {
			printLogs(c, 2);
			lk.unlock();

			int driveTime = calculateDriveTime(c);
			if (direction == "N" || direction == "S") {
				lock_guard<mutex> lock1(inter00);
				lock_guard<mutex> lock2(inter10);
				this_thread::sleep_for(chrono::seconds(driveTime));
			}
			else if (direction == "E" || direction == "W") {
				lock_guard<mutex> lock1(inter01);
				lock_guard<mutex> lock2(inter11);
				this_thread::sleep_for(chrono::seconds(driveTime));
			}

			lk.lock();
			printLogs(c, 3);
			carQueue->pop();
			break;

		}
		else {
			lk.unlock();
			this_thread::sleep_for(chrono::seconds(1));
		}
	}

}

void processCar1(Car c, string direction) {
	string trafficLightKey = (direction == "N" || direction == "S") ? "north-south" : "east-west";
	auto& light = trafficLights[trafficLightKey];

	unique_lock<mutex> lk(light.mtx);

	printLogs(c, 1);

	this_thread::sleep_for(chrono::seconds(c.time)); 


	light.cv.wait(lk, [&light] { return light.state == LightState::GREEN; });

	printLogs(c, 2);  

	int driveTime = calculateDriveTime(c);
	this_thread::sleep_for(chrono::seconds(driveTime));  

	printLogs(c, 3);  
}




bool fileExists(string filename) {
	ifstream ifile(filename);
	return (bool)ifile;
}


int main() {
	if (fileExists("outFile.txt")) {
		remove("outFile.txt");
	}

	outFile.open("outFile.txt");
	vector<thread> threads;
	dict['N'] = 'S';
	dict['S'] = 'N';
	dict['E'] = 'W';
	dict['W'] = 'E';

	string fileName;
	cout << "input file name (test, test2 , test3, test4 or test5):  ";
	cin >> fileName;

	outFile << "Output File for input file: " << fileName << "\n" << "\n";
	fileReader(fileName);



	// Start traffic light controller in a separate thread
	thread lightControllerThread(trafficLightController);
	
	auto start = chrono::steady_clock::now();

	for (auto& car : cars) {
		string direction;
		direction = car.origin;
		threads.push_back(thread(processCar1, car, direction));

	}
	for (auto& t : threads) {
		t.join();
	}


	auto end = chrono::steady_clock::now();

	if (lightControllerThread.joinable()) {
		lightControllerThread.detach(); 
	}
	cout << "execution took " << chrono::duration_cast<chrono::seconds>(end - start).count() << "seconds." << endl;
	outFile << "execution took " << chrono::duration_cast<chrono::seconds>(end - start).count() << "seconds.";
	outFile.close();

	return 0;
}