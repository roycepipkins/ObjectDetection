#include "PersonDetection.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <iostream>
#include <iomanip>
#include "Detector.h"


POCO_SERVER_MAIN(PersonDetection);




using namespace cv;
using namespace std;


void PersonDetection::initialize(Application& self)
{
	ServerApplication::initialize(self);
}

void PersonDetection::uninitialize()
{
}

int PersonDetection::main(const std::vector<std::string>& args)
{
    std::string url = "rtsp://admin:admin@192.168.1.250:554/cam/realmonitor?channel=1&subtype=1&unicast=true&proto=Onvif";

    //cv::VideoCapture capture(url);
    cv::VideoCapture capture("307Schiller_PipkinsTireTheftCombined.mp4");


    Poco::Thread::sleep(2000);

    if (!capture.isOpened()) {
        return -1;
    }

    Detector detector;

    cv::namedWindow("TEST", cv::WindowFlags::WINDOW_AUTOSIZE);

    cv::Mat big_frame;
    cv::Mat frame;

    for (;;)
    {
        capture >> big_frame;
        if (big_frame.empty())
        {
            cout << "Finished reading: empty frame" << endl;
            break;
        }
        else
        {
            cv::resize(big_frame, frame, cv::Size(800, 600));
        }
        
        int64 t = getTickCount();
        vector<Rect> found = detector.detect(frame);
        t = getTickCount() - t;

        // show the window
        {
            ostringstream buf;
            buf << "Mode: " << detector.modeName() << " ||| "
                << "FPS: " << fixed << setprecision(1) << (getTickFrequency() / (double)t);
            putText(frame, buf.str(), Point(10, 30), FONT_HERSHEY_PLAIN, 2.0, Scalar(0, 0, 255), 2, LINE_AA);
        }
        for (vector<Rect>::iterator i = found.begin(); i != found.end(); ++i)
        {
            Rect& r = *i;
            detector.adjustRect(r);
            rectangle(frame, r.tl(), r.br(), cv::Scalar(0, 255, 0), 2);
        }
        imshow("People detector", frame);

        // interact with user
        const char key = (char)waitKey(1);
        if (key == 27 || key == 'q') // ESC
        {
            cout << "Exit requested" << endl;
            break;
        }
        else if (key == ' ')
        {
            detector.toggleMode();
        }
    }

	return 0;
}
