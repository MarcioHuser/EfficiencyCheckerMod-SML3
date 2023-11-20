#pragma once

#include "ProductionDetail.generated.h"

USTRUCT(Blueprintable)
struct EFFICIENCYCHECKERMOD_API FProductionDetail
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="EfficiencyCheckerLogic|ProductionDetail")
	TSubclassOf<class UFGItemDescriptor> buildingType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="EfficiencyCheckerLogic|ProductionDetail")
	TSubclassOf<class UFGRecipe> recipe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="EfficiencyCheckerLogic|ProductionDetail")
	float multiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="EfficiencyCheckerLogic|ProductionDetail")
	int amount = 0;

public:
	// FORCEINLINE ~FProductionDetail() = default;
};
