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
		if (detection.is_null)
		{
			filteredDetections.push_back(detection);
		}
		else
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
	}

	if (filteredDetections.empty())
	{
		Detection null_detection;
		null_detection.src_name = detections.at(0).src_name;
		filteredDetections.push_back(null_detection);
	}

	filteredDetectionEvent.notify(this, filteredDetections);
}
