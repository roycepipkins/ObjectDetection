#pragma once
#include <Poco/Util/ServerApplication.h>

namespace cv{ class Mat; }


class PersonDetection : public Poco::Util::ServerApplication
{
protected:
	void initialize(Application& self);
	void uninitialize();
	int main(const std::vector < std::string >& args);

	std::vector<std::string> classes;
	void drawPred(int classId, float conf, int left, int top, int right, int bottom, cv::Mat& frame);
};

