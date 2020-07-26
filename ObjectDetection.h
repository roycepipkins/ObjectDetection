#pragma once
#include <Poco/SharedPtr.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/LogStream.h>
#include "Detector.h"
#include "EventFilter.h"
#include "MqttEmitter.h"
#include <opencv2/core/utils/logger.hpp>


class ObjectDetection : public Poco::Util::ServerApplication
{
protected:
	void initialize(Application& self);
	void uninitialize();
	int main(const std::vector < std::string >& args);

	void ConfigureLogging();

	void SetupCameras();
	void SetupMQTT();
	void SetupURLs();
	void StartupCameras();
	void StartupMQTT();
	void StartupURLs();

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

	Poco::SharedPtr<Poco::LogStream> opencv_cout;
	Poco::SharedPtr<Poco::LogStream> opencv_cerr;

	cv::utils::logging::LogLevel StrToLogLevel(const std::string& log_level);
};

