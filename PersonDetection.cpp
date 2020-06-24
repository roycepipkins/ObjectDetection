#include "PersonDetection.h"

#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/Logger.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/AsyncChannel.h>
#include <Poco/FormattingChannel.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/dnn.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>

#include "Detector.h"



POCO_SERVER_MAIN(PersonDetection);




using namespace cv;
using namespace std;
using namespace Poco;


void PersonDetection::initialize(Application& self)
{
    loadConfiguration();
	ServerApplication::initialize(self);
    ConfigureLogging();
    Poco::Logger::root().information("---------- Startup ----------");
}

void PersonDetection::uninitialize()
{
    Poco::Logger::root().information("---------- Shutdown ----------");
    Poco::Thread::sleep(100);
    Poco::Logger::shutdown();
}


int PersonDetection::main(const std::vector<std::string>& args)
{
    
    try
    {
        //std::string url = "rtsp://admin:admin@192.168.1.250:554/cam/realmonitor?channel=1&subtype=1&unicast=true&proto=Onvif";


        vector<SharedPtr<Detector>> detectors;
        vector<string> cameras;
        config().keys("camera", cameras);

        for (auto camera : cameras)
        {
            auto camera_config = config().createView("camera." + camera);
            SharedPtr<Detector> detector = new Detector(camera, isInteractive(), camera_config);
            detectors.push_back(detector);
            
        }

        waitForTerminationRequest();

        

    }
    catch (std::exception& e)
    {
        Poco::Logger::root().fatal(e.what());
        return -1;
    }
    

	return 0;
}


void PersonDetection::ConfigureLogging()
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
