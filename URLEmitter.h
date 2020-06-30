#pragma once
#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/URI.h>
#include <Poco/Logger.h>

#include <vector>

#include "Detection.h"
#include "ThreadedDetectionProcessor.h"

class URLEmitter : public ThreadedDetectionProcessor
{
public:
	URLEmitter(const std::string url);
	
	void processDetection(std::vector<Detection>& detections);

private:
	std::string theUrl;
	Poco::URI uri;

	Poco::Logger& log;
};

