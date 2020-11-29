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
	
	void start() override;
	cv::Mat GetNextFrame(const int wait_ms = 100) override;
	void stop() override;
private:
	Poco::DirectoryWatcher watcher;
	Poco::Mutex mu_new_file_que;
	std::queue<Poco::File> new_file_que;
	Poco::Event file_added;
	volatile bool want_to_stop;
	bool WaitForFileToComplete(const Poco::File& file, const int timeout_ms = 10000);
};

