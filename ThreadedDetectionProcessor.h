#pragma once
#include <vector>
#include <queue>

#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/Mutex.h>
#include <Poco/Event.h>
#include <Poco/BasicEvent.h>

#include "Detection.h"
class ThreadedDetectionProcessor : public Poco::Runnable
{
public:
	virtual ~ThreadedDetectionProcessor();

	void onDetection(const void* sender, std::vector<Detection>& detections);
	void start();
	virtual void run();
	void stop();

protected:
	virtual void processDetection(std::vector<Detection>& detections) = 0;

private:
	std::queue<std::vector<Detection>> detection_queue;
	Poco::Mutex mu_detection_queue;
	Poco::Event evt_detection_queue;
	volatile bool want_to_stop = false;
	Poco::Thread processor_thread;
};

