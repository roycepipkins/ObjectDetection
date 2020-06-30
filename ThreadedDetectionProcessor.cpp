#include "ThreadedDetectionProcessor.h"

using namespace Poco;
using namespace std;

ThreadedDetectionProcessor::~ThreadedDetectionProcessor()
{
	stop();
}

void ThreadedDetectionProcessor::onDetection(const void* sender, std::vector<Detection>& detections)
{
	ScopedLock<Mutex> locker(mu_detection_queue);
	detection_queue.push(detections);
	evt_detection_queue.set();
}

void ThreadedDetectionProcessor::start()
{
	if (!processor_thread.isRunning()) processor_thread.start(*this);
}

void ThreadedDetectionProcessor::run()
{
	while (!want_to_stop)
	{
		if (evt_detection_queue.tryWait(500))
		{
			ScopedLock<Mutex> locker(mu_detection_queue);
			auto detection = detection_queue.front();
			processDetection(detection);
			detection_queue.pop();
		}
	}
}

void ThreadedDetectionProcessor::stop()
{
	want_to_stop = true;
	if (processor_thread.isRunning())
		processor_thread.join();
}
