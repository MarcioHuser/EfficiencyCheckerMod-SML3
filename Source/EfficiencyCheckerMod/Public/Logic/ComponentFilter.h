#pragma once
#include "Resources/FGItemDescriptor.h"

#include "ComponentFilter.generated.h"

USTRUCT(Blueprintable)
struct EFFICIENCYCHECKERMOD_API FComponentFilter
{
	GENERATED_BODY()
public:
	virtual ~FComponentFilter()
	{
	}

	virtual bool itemIsAllowed(const TSubclassOf<UFGItemDescriptor>& item) const;

	virtual TSet<TSubclassOf<UFGItemDescriptor>> filterItems(const TSet<TSubclassOf<UFGItemDescriptor>>& items) const;

	static FComponentFilter combineFilters(const FComponentFilter& filter1, const FComponentFilter& filter2);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ComponentFilter|ProductionDetail")
	TSet<TSubclassOf<UFGItemDescriptor>> allowedItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ComponentFilter|ProductionDetail")
	bool allowedFiltered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ComponentFilter|ProductionDetail")
	TSet<TSubclassOf<UFGItemDescriptor>> deniedItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ComponentFilter|ProductionDetail")
	bool deniedFiltered = false;
public:
};
