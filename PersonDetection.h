#pragma once
#include <Poco/SharedPtr.h>
#include <Poco/Util/ServerApplication.h>
#include "Detector.h"
#include "EventFilter.h"
#include "MqttEmitter.h"

class PersonDetection : public Poco::Util::ServerApplication
{
protected:
	void initialize(Application& self);
	void uninitialize();
	int main(const std::vector < std::string >& args);

	void ConfigureLogging();

	void SetupCameras();
	void SetupMQTT();
	void SetupURLs();
	void ShutdownCameras();
	void ShutdownMQTT();
	void ShutdownURLs();

	std::map<std::string, Poco::SharedPtr<Detector>> detectors;

private:
	Poco::SharedPtr<ThreadedDetectionProcessor> mqtt;
	Poco::SharedPtr<EventFilter> mqtt_filter;

	struct URLEventProcessor
	{
		Poco::SharedPtr<ThreadedDetectionProcessor> url;
		Poco::SharedPtr<EventFilter> url_filter;
	};

	std::unordered_map<std::string, URLEventProcessor> urls;

};

