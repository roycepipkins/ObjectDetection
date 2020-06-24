#pragma once
#include <Poco/Util/ServerApplication.h>


class PersonDetection : public Poco::Util::ServerApplication
{
protected:
	void initialize(Application& self);
	void uninitialize();
	int main(const std::vector < std::string >& args);

	void ConfigureLogging();
};

