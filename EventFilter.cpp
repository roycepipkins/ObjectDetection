#include "EventFilter.h"

EventFilter::EventFilter(const std::vector<StringFilter> filterValues) :
	filters(filterValues)
{
}

void EventFilter::onDetectionEvent(const void* sender, std::vector<Detection>& detections)
{
	std::vector<Detection> filteredDetections;

	for (auto detection : detections)
	{
		bool passed = false;
		bool negated = false;
		for (const auto& filter : filters)
		{
			if (filter.isNegatation()) negated |= filter.match(detection.Name());
			else passed |= filter.match(detection.Name());
		}

		if (passed && !negated) filteredDetections.push_back(detection);
	}

	if (!filteredDetections.empty())
		filteredDetectionEvent.notify(this, filteredDetections);
}
