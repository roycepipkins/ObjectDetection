#include "PersonDetection.h"

#include <Poco/Path.h>
#include <Poco/File.h>


#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <iomanip>
#include <fstream>



POCO_SERVER_MAIN(PersonDetection);




using namespace cv;
using namespace std;
//using namespace Poco;

void PersonDetection::initialize(Application& self)
{
	ServerApplication::initialize(self);
}

void PersonDetection::uninitialize()
{
}

void PersonDetection::drawPred(int classId, float conf, int left, int top, int right, int bottom, Mat& frame)
{
    rectangle(frame, Point(left, top), Point(right, bottom), Scalar(0, 255, 0));

    std::string label = format("%.2f", conf);
    if (!classes.empty())
    {
        CV_Assert(classId < (int)classes.size());
        label = classes[classId] + ": " + label;
    }

    int baseLine;
    Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    top = max(top, labelSize.height);
    rectangle(frame, Point(left, top - labelSize.height),
        Point(left + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
    putText(frame, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());
}

int PersonDetection::main(const std::vector<std::string>& args)
{
    std::string url = "rtsp://admin:admin@192.168.1.250:554/cam/realmonitor?channel=1&subtype=1&unicast=true&proto=Onvif";

    Poco::Path yolo_config_path(config().getString("application.dir"));
    yolo_config_path.append("yolo-coco");
    Poco::Path yolo_weight_path(yolo_config_path);
    Poco::Path coco_names_path(yolo_config_path);
    yolo_config_path.append("yolov3.cfg");
    yolo_weight_path.append("yolov3.weights");
    coco_names_path.append("coco.names");

    std::ifstream ifs(coco_names_path.toString().c_str());
    //if (!ifs.is_open())
    //    CV_Error(Error::StsError, "File " + file + " not found");
    std::string line;
    while (std::getline(ifs, line))
    {
        classes.push_back(line);
    }



    auto yolo_net = dnn::readNetFromDarknet(yolo_config_path.toString(), yolo_weight_path.toString());

    
    //cv::VideoCapture capture(url);
    cv::VideoCapture capture("307Schiller_PipkinsTireTheftCombined.mp4");


    if (!capture.isOpened()) {
        return -1;
    }

    //capture.set(CAP_PROP_POS_MSEC, 200000);

    cv::namedWindow("TEST", cv::WindowFlags::WINDOW_AUTOSIZE);

    cv::Mat big_frame;
    cv::Mat frame;

    for (;;)
    {
        //10 FPS on the tire theft movie. Lets try 2 frames.
        for(int s = 0; s < 5; ++s)
            capture >> big_frame;


        if (big_frame.empty())
        {
            cout << "Finished reading: empty frame" << endl;
            break;
        }
        else
        {
            cv::resize(big_frame, frame, cv::Size(640, 480));
        }
        

        auto output_layers = yolo_net.getUnconnectedOutLayersNames();
        vector<vector<Mat>> outputs;
        int64 t = getTickCount(); //--- timer start -----
        auto blob_img = dnn::blobFromImage(frame, 1.0 / 255.0, Size(416, 416), Scalar(), true, false);
        yolo_net.setInput(blob_img);
        yolo_net.forward(outputs, output_layers);
        t = getTickCount() - t; //--- timer stop -----

        // show the window
        {
            ostringstream buf;
            buf << "Mode: " << "YOLOV3" << " ||| "
                << "FPS: " << fixed << setprecision(1) << (getTickFrequency() / (double)t);
            putText(frame, buf.str(), Point(10, 30), FONT_HERSHEY_PLAIN, 2.0, Scalar(0, 0, 255), 2, LINE_AA);
        }

        float confidenceThreshold = 0.5;
        float nmsThreshold = 0.3;
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<Rect> boxes;


        for (auto output : outputs)
        {
            for (auto detection : output)
            {
                float* data = (float*)detection.data;
                for (int j = 0; j < detection.rows; ++j, data += detection.cols)
                {
                    Mat scores = detection.row(j).colRange(5, detection.cols);
                    Point classIdPoint;
                    double confidence;
                    minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                    if (confidence > confidenceThreshold)
                    {
                        int centerX = (int)(data[0] * frame.cols);
                        int centerY = (int)(data[1] * frame.rows);
                        int width = (int)(data[2] * frame.cols);
                        int height = (int)(data[3] * frame.rows);
                        int left = centerX - width / 2;
                        int top = centerY - height / 2;

                        classIds.push_back(classIdPoint.x);
                        confidences.push_back((float)confidence);
                        boxes.push_back(Rect(left, top, width, height));
                    }
                }
            }
        }


        std::map<int, std::vector<size_t> > class2indices;
        for (size_t i = 0; i < classIds.size(); i++)
        {
            if (confidences[i] >= confidenceThreshold)
            {
                class2indices[classIds[i]].push_back(i);
            }
        }
        std::vector<Rect> nmsBoxes;
        std::vector<float> nmsConfidences;
        std::vector<int> nmsClassIds;
        for (std::map<int, std::vector<size_t> >::iterator it = class2indices.begin(); it != class2indices.end(); ++it)
        {
            std::vector<Rect> localBoxes;
            std::vector<float> localConfidences;
            std::vector<size_t> classIndices = it->second;
            for (size_t i = 0; i < classIndices.size(); i++)
            {
                localBoxes.push_back(boxes[classIndices[i]]);
                localConfidences.push_back(confidences[classIndices[i]]);
            }
            std::vector<int> nmsIndices;
            dnn::NMSBoxes(localBoxes, localConfidences, confidenceThreshold, nmsThreshold, nmsIndices);
            for (size_t i = 0; i < nmsIndices.size(); i++)
            {
                size_t idx = nmsIndices[i];
                nmsBoxes.push_back(localBoxes[idx]);
                nmsConfidences.push_back(localConfidences[idx]);
                nmsClassIds.push_back(it->first);
            }
        }
        boxes = nmsBoxes;
        classIds = nmsClassIds;
        confidences = nmsConfidences;
    

        for (size_t idx = 0; idx < boxes.size(); ++idx)
        {
            Rect box = boxes[idx];
            drawPred(classIds[idx], confidences[idx], box.x, box.y,
                box.x + box.width, box.y + box.height, frame);
        }


        
        imshow("People detector", frame);

        // interact with user
        const char key = (char)waitKey(1);
        if (key == 27 || key == 'q') // ESC
        {
            cout << "Exit requested" << endl;
            break;
        }
        
        
    }

	return 0;
}
