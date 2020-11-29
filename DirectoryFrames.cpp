#include "DirectoryFrames.h"
#include <Poco/Delegate.h>
#include <Poco/Timestamp.h>
#include <opencv2/opencv.hpp>

using namespace Poco;

DirectoryFrames::DirectoryFrames(const std::string& directory_path, const int scan_interval ):
watcher(directory_path, DirectoryWatcher::DW_ITEM_ADDED, scan_interval),
want_to_stop(false)
{
	watcher.itemAdded += delegate(this, &DirectoryFrames::onItemAdded);
}

void DirectoryFrames::onItemAdded(const void* sender, const Poco::DirectoryWatcher::DirectoryEvent& directoryEvent)
{
	ScopedLock<Mutex> locker(mu_new_file_que);
	new_file_que.push(directoryEvent.item);
	file_added.set();
}

void DirectoryFrames::start()
{
	want_to_stop = false;
}

void DirectoryFrames::stop()
{
	want_to_stop = true;
	//TODO remove the delegate?
}

cv::Mat DirectoryFrames::GetNextFrame(const int wait_ms)
{
	cv::Mat empty_frame;
	while (!want_to_stop)
	{
		if (file_added.tryWait(wait_ms))
		{
			File added_file;
			{
				ScopedLock<Mutex> locker(mu_new_file_que);
				added_file = new_file_que.front();
				new_file_que.pop();
			}

			if (added_file.exists())
			{
				return cv::imread(added_file.path());
			}
		}
	}

	return empty_frame;
}
