#include "DirectoryFrames.h"
#include <Poco/Delegate.h>
#include <Poco/Timestamp.h>
#include <Poco/FileStream.h>
#include <Poco/Logger.h>
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
				if (WaitForFileToComplete(added_file))
					return cv::imread(added_file.path());
			}
		}
		else
		{
			cv::waitKey(1);
		}
		
	}

	return empty_frame;
}

bool DirectoryFrames::WaitForFileToComplete(const Poco::File& file, const int timeout_ms)
{
	Timestamp timeout;
	while (!want_to_stop && timeout.elapsed() < timeout_ms * 1000)
	{
		try
		{
			FileStream file_lock(file.path());
			return true;
		}
		catch (Poco::Exception& e)
		{
			Thread::sleep(100);
		}
		cv::waitKey(1);
	}
	return false;
}