#pragma once
#include <vector>
#include "StringFilter.h"
#include "Detection.h"
#include <Poco/BasicEvent.h>
class EventFilter
{
public:
	EventFilter(const std::vector<StringFilter> classFilterValues, const std::vector<StringFilter> sourceFilterValues);

	void onDetectionEvent(const void* sender, std::vector<Detection>& detections);
	Poco::BasicEvent<std::vector<Detection>> filteredDetectionEvent;
private:
	std::vector<StringFilter> classFilters;
	std::vector<StringFilter> sourceFilters;
};

