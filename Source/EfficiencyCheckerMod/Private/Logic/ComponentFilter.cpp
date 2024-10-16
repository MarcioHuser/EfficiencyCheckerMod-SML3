#include "Logic/ComponentFilter.h"
#include "Util/ECMOptimize.h"
#include "Logic/EfficiencyCheckerLogic.h"
#include "Logic/EfficiencyCheckerLogic2.h"
#include "Subsystems/CommonInfoSubsystem.h"

#ifndef OPTIMIZE
#pragma optimize("", off )
#endif

bool FComponentFilter::itemIsAllowed(ACommonInfoSubsystem* commonInfoSubsystem, const TSubclassOf<UFGItemDescriptor>& item) const
{
	if (allowedFiltered &&
		!allowedItems.Contains(item) &&
		allowedItems.Intersect(commonInfoSubsystem->overflowItemDescriptors).IsEmpty() &&
		allowedItems.Intersect(commonInfoSubsystem->wildCardItemDescriptors).IsEmpty())
	{
		return false;
	}

	if (deniedFiltered && deniedItems.Contains(item))
	{
		return false;
	}

	return true;
}

TSet<TSubclassOf<UFGItemDescriptor>> FComponentFilter::filterItems(const TSet<TSubclassOf<UFGItemDescriptor>>& items) const
{
	TSet<TSubclassOf<UFGItemDescriptor>> tempItems = items;

	if (allowedFiltered)
	{
		tempItems = tempItems.Intersect(allowedItems);
	}

	if (deniedFiltered)
	{
		tempItems = tempItems.Difference(deniedItems);
	}

	return tempItems;
}

FComponentFilter FComponentFilter::combineFilters(const FComponentFilter& filter1, const FComponentFilter& filter2)
{
	FComponentFilter combinedFilter = filter1;

	if (combinedFilter.allowedFiltered && filter2.allowedFiltered)
	{
		// Combine filters
		combinedFilter.allowedItems = filter2.filterItems(combinedFilter.allowedItems);
	}
	else if (filter2.allowedFiltered)
	{
		// Define filters
		combinedFilter.allowedFiltered = true;
		combinedFilter.allowedItems = filter2.allowedItems;
	}

	if (combinedFilter.deniedFiltered && filter1.deniedFiltered)
	{
		// Combine filters
		combinedFilter.deniedItems.Append(filter2.deniedItems);
	}
	else if (filter2.deniedFiltered)
	{
		// Define filters
		combinedFilter.deniedFiltered = true;
		combinedFilter.deniedItems = filter2.deniedItems;
	}

	return combinedFilter;
}

#ifndef OPTIMIZE
#pragma optimize("", on)
#endif
