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
	URLEmitter(const std::string& emitter_name, const std::string& url, const std::string& username = "", const std::string& password = "", const bool log_detections = false);
	
	void processDetection(std::vector<Detection>& detections);

private:
	std::string name;
	std::string theUrl;
	std::string user;
	std::string pw;
	Poco::URI uri;
	bool log_detects;

	Poco::Logger& log;

	void LogDetection(std::vector<Detection>& detections);
};

