#pragma once
#include "Equipment/FGEquipment.h"

#include "EfficiencyCheckerEquipment.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FShowStatsWidgetEvent,
	float,
	injectedInput,
	float,
	limitedThroughput,
	float,
	requiredOutput,
	const TArray<TSubclassOf<UFGItemDescriptor>>&,
	injectedItems,
	bool,
	overflow
	);

UCLASS(BlueprintType)
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerEquipment : public AFGEquipment
{
	GENERATED_BODY()
public:
	AEfficiencyCheckerEquipment();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	virtual void PrimaryFirePressed(class AFGBuildable* targetBuildable);
	virtual void PrimaryFirePressed_Server(class AFGBuildable* targetBuildable);

	UPROPERTY(BlueprintAssignable, Category = "EfficiencyChecker")
	FShowStatsWidgetEvent OnShowStatsWidget;

	UFUNCTION(Category = "EfficiencyChecker", NetMulticast, Reliable)
	virtual void ShowStatsWidget
	(
		UPARAM(DisplayName = "Injected Input") float in_injectedInput,
		UPARAM(DisplayName = "Limited Throughput") float in_limitedThroughput,
		UPARAM(DisplayName = "Required Output") float in_requiredOutput,
		UPARAM(DisplayName = "Items") const TArray<TSubclassOf<UFGItemDescriptor>>& in_injectedItems,
		UPARAM(DisplayName = "Overflow") bool in_overflow
	);

	static FString getAuthorityAndPlayer(const AActor* actor);
	
	FString _TAG_NAME = TEXT("EfficiencyCheckerEquipment: ");

	// inline static FString
	// getTimeStamp()
	// {
	// 	const auto now = FDateTime::Now();
	//
	// 	return FString::Printf(TEXT("%02d:%02d:%02d"), now.GetHour(), now.GetMinute(), now.GetSecond());
	// }

	inline FString
	getTagName() const
	{
		return /*getTimeStamp() + TEXT(" ") +*/ _TAG_NAME;
	}

public:
	FORCEINLINE ~AEfficiencyCheckerEquipment() = default;
};
