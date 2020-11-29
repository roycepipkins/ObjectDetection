#pragma once
#include <Poco/SharedPtr.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/LogStream.h>

#include <opencv2/core/utils/logger.hpp>

#include "Detector.h"
#include "SourceDetectionManager.h"
#include "EventFilter.h"
#include "MqttEmitter.h"

class ObjectDetection : public Poco::Util::ServerApplication
{
protected:
	void initialize(Application& self);
	void uninitialize();
	int main(const std::vector < std::string >& args);

	void ConfigureLogging();

	void SetupDetector();
	void SetupCameras();
	void SetupMQTT();
	void SetupURLs();

	void StartupDetector();
	void StartupCameras();
	void StartupMQTT();
	void StartupURLs();

	void ShutdownDetector();
	void ShutdownCameras();
	void ShutdownMQTT();
	void ShutdownURLs();

	

private:
	Poco::SharedPtr<Detector> detector;
	std::map<std::string, Poco::AutoPtr<SourceDetectionManager>> managers;

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

	Poco::AutoPtr<FrameSource> CreateFrameSource(Poco::Util::AbstractConfiguration::Ptr config);
};

