// ReSharper disable CppUE4CodingStandardNamingViolationWarning

#pragma once

#include "Hologram/FGBuildableHologram.h"

#include "EfficiencyCheckerHologram.generated.h"

/**
 * 
 */
UCLASS()
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerHologram : public AFGBuildableHologram
{
	GENERATED_BODY()

	//// AFGHologram interface
	///** Net Construction Messages */
	//virtual void SerializeConstructMessage(FArchive& ar, FNetConstructionID id) override;
	virtual bool IsValidHitResult(const FHitResult& hitResult) const override;
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
	virtual void AdjustForGround(FVector& out_adjustedLocation, FRotator& out_adjustedRotation) override;
	//virtual AActor* Construct(TArray< AActor* >& out_children, FNetConstructionID netConstructionID) override;
	virtual void ScrollRotate(int32 delta, int32 step) override;
	//// End AFGHologram interface

	virtual void CheckValidPlacement() override;

	//virtual void GetSupportedScrollModes(TArray<EHologramScrollMode>* out_modes) const override;

	//virtual int32 GetRotationStep() const override;

	virtual void ConfigureComponents(class AFGBuildable* inBuildable) const override;

	static FString getAuthorityAndPlayer(const AActor* actor);

	/**
	 * See if we can snap to the hit actor. Used for holograms snapping on top of ex resource nodes, other buildings like stackable poles and similar.
	   If returning true, we assume location and snapping is applied, and no further location and rotation will be updated this frame by the build gun.
	 * @return true if we can snap; false if not.
	 */
	// virtual bool TrySnapToActor(const FHitResult& hitResult) override;

	FString _TAG_NAME = TEXT("EfficiencyCheckerHologram: ");

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

	class AEfficiencyCheckerBuilding* GetCheckerBuildable() const;

public:
	AEfficiencyCheckerHologram();

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// ReSharper disable once IdentifierTypo
	virtual void dumpDisqualifiers() const;

	float splitOffset = 0;
	int rotationDelta = 0;

	UPROPERTY()
	class AEfficiencyCheckerBuilding* efficiencyChecker = nullptr;

	UPROPERTY()
	class AFGBuildableConveyorBelt* conveyor = nullptr;

	UPROPERTY()
	class AFGBuildablePipeline* pipeline = nullptr;

    FHitResult lastHit_;
};
