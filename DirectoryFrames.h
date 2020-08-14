#pragma once
#include <Poco/DirectoryWatcher.h>
#include <Poco/File.h>
#include <Poco/Event.h>
#include <Poco/Mutex.h>
#include <Poco/Thread.h>
#include <queue>
#include "FrameSource.h"

class DirectoryFrames : public FrameSource
{
public:
	DirectoryFrames(const std::string& directory_path, const int scan_interval = Poco::DirectoryWatcher::DW_DEFAULT_SCAN_INTERVAL);

	void onItemAdded(const void* sender, const Poco::DirectoryWatcher::DirectoryEvent& directoryEvent);
	
	cv::Mat GetNextFrame();
	void stop();
private:
	Poco::DirectoryWatcher watcher;
	Poco::Mutex mu_new_file_que;
	std::queue<Poco::File> new_file_que;
	Poco::Event file_added;
	volatile bool want_to_stop;
};

