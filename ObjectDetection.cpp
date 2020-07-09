#include "ObjectDetection.h"
#include "StringFilter.h"
#include "MqttEmitter.h"
#include "URLEmitter.h"

#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/Logger.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/AsyncChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Delegate.h>
#include <Poco/BasicEvent.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/dnn.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>

POCO_SERVER_MAIN(ObjectDetection);

using namespace cv;
using namespace std;
using namespace Poco;


void ObjectDetection::initialize(Application& self)
{
    loadConfiguration();
	ServerApplication::initialize(self);
    ConfigureLogging();
    Poco::Logger::root().information("---------- Startup ----------");
}

void ObjectDetection::uninitialize()
{
    Poco::Logger::root().information("---------- Shutdown ----------");
    Poco::Thread::sleep(100);
    Poco::Logger::shutdown();
}


int ObjectDetection::main(const std::vector<std::string>& args)
{
    
    try
    {
        SetupCameras();
        SetupMQTT();
        SetupURLs();

        StartupMQTT();
        StartupURLs();
        StartupCameras();

        waitForTerminationRequest();

        ShutdownCameras();
        ShutdownMQTT();
        ShutdownURLs();

    }
    catch (std::exception& e)
    {
        Poco::Logger::root().fatal(e.what());
        return -1;
    }
    

	return 0;
}


void ObjectDetection::SetupCameras()
{
    
    vector<string> cameras;
    config().keys("camera", cameras);

    for (auto camera : cameras)
    {
        auto camera_config = config().createView("camera." + camera);
        SharedPtr<Detector> detector = new Detector(camera, isInteractive(), camera_config);
        detectors[camera] = detector;
    }
}

void ObjectDetection::SetupMQTT()
{
    //Filter format
    //mqtt.filter = camname.classname, camname.cla*, !camname.*, !*.classname
    
    if (config().has("mqtt.broker_address"))
    {

        mqtt = new MqttEmitter(
            config().getString("mqtt.broker_address"),
            config().getString("mqtt.username", ""),
            config().getString("mqtt.password", ""),
            config().getBool  ("mqtt.use_ssl", false),
            config().getString("mqtt.topic_prefix", ""),
            config().getString("mqtt.clientid", config().getString("application.baseName")),
            config().getInt("mqtt.qos", 1)
        );



        if (config().has("mqtt.class_filter") || config().has("mqtt.source_filter"))
        {
            vector<StringFilter> mqtt_class_filters;
            StringTokenizer tokenizer(config().getString("mqtt.class_filter", "*"), ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
            for (auto filter : tokenizer)
            {
                mqtt_class_filters.emplace_back(filter, false);
            }

            vector<StringFilter> mqtt_source_filters;
            StringTokenizer src_tokenizer(config().getString("mqtt.source_filter", "*"), ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
            for (auto filter : src_tokenizer)
            {
                mqtt_source_filters.emplace_back(filter, false);
            }


            mqtt_filter = new EventFilter(mqtt_class_filters, mqtt_source_filters);

            for (auto& [name, detector] : detectors)
            {
                detector->detectionEvent += delegate(mqtt_filter.get(), &EventFilter::onDetectionEvent);
            }

            mqtt_filter->filteredDetectionEvent += delegate(mqtt.get(), &ThreadedDetectionProcessor::onDetection);

        }
        else
        {
            for (auto& [name, detector] : detectors)
            {
                detector->detectionEvent += delegate(mqtt.get(), &ThreadedDetectionProcessor::onDetection);
            }
        }

        mqtt->start();
    }
    
}

void ObjectDetection::SetupURLs()
{
    const string url_config_key = "url_fetch";

    vector<string> url_config_names;
    config().keys(url_config_key, url_config_names);

    for (auto url_name : url_config_names)
    {
        URLEventProcessor eventProcessor;
        eventProcessor.url = new URLEmitter(
            config().getString(url_config_key + "." + url_name + ".url"),
            config().getString(url_config_key + "." + url_name + ".username", ""),
            config().getString(url_config_key + "." + url_name + ".password", ""));


        if (config().has(url_config_key + "." + url_name + ".class_filter") || config().has(url_config_key + "." + url_name + ".source_filter"))
        {
            vector<StringFilter> url_class_filters;
            StringTokenizer tokenizer(config().getString(url_config_key + "." + url_name + ".class_filter", "*"), ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
            for (auto filter : tokenizer)
            {
                url_class_filters.emplace_back(filter, false);
            }


            vector<StringFilter> url_source_filters;
            StringTokenizer source_tokenizer(config().getString(url_config_key + "." + url_name + ".source_filter", "*"), ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
            for (auto filter : source_tokenizer)
            {
                url_source_filters.emplace_back(filter, false);
            }

            eventProcessor.url_filter = new EventFilter(url_class_filters, url_source_filters);

            for (auto& [name, detector] : detectors)
            {
                detector->detectionEvent += delegate(eventProcessor.url_filter.get(), &EventFilter::onDetectionEvent);
            }

            eventProcessor.url_filter->filteredDetectionEvent += delegate(eventProcessor.url.get(), &ThreadedDetectionProcessor::onDetection);
        }
        else
        {
            for (auto& [name, detector] : detectors)
            {
                detector->detectionEvent += delegate(eventProcessor.url.get(), &ThreadedDetectionProcessor::onDetection);
            }
        }

        urls[url_name] = eventProcessor;
    }
}

void ObjectDetection::StartupCameras()
{
    for (auto& [name, detector] : detectors)
    {
        detector->start();
    }
}

void ObjectDetection::StartupMQTT()
{
    if (!mqtt.isNull()) mqtt->start();
}

void ObjectDetection::StartupURLs()
{
    for (auto& [name, url] : urls)
    {
        url.url->start();
    }
}

void ObjectDetection::ShutdownCameras()
{
    for (auto& [name, detector] : detectors)
    {
        detector->stop();
    }
}

void ObjectDetection::ShutdownMQTT()
{
    if (!mqtt.isNull()) mqtt->stop();
}

void ObjectDetection::ShutdownURLs()
{
    for (auto& [name, url] : urls)
    {
        url.url->stop();
    }
}

void ObjectDetection::ConfigureLogging()
{
    try
    {
        using namespace Poco;


        Path logpath = config().getString("application.dir");
        logpath.append("logs");
        File(logpath).createDirectories();
        logpath.append(config().getString("application.baseName") + ".log");

        if (config().has("logs.full_path_override"))
        {
            logpath = config().getString("logs.full_path_override");
        }


        AutoPtr<FileChannel> fc(new FileChannel);
        fc->setProperty("path", logpath.toString());
        fc->setProperty("rotation", config().getString("logs.rotation", "00:00"));
        fc->setProperty("archive", "timestamp");
        fc->setProperty("times", config().getBool("logs.use_utc", false) ? "local" : "utc");
        fc->setProperty("purgeAge", config().getString("logs.purge_age", "12 months"));

        AutoPtr<PatternFormatter> pf(new PatternFormatter);
        pf->setProperty("pattern", "%Y-%m-%d %H:%M:%S.%i %p: %s %t");
        AutoPtr<FormattingChannel> fmtc(new FormattingChannel(pf, fc));

        Logger::root().setChannel(fmtc);
        Logger::root().setLevel(config().getString("log.level", "information"));
    }
    catch (std::exception& e)
    {
        std::cerr << e.what();
    }
    

}
