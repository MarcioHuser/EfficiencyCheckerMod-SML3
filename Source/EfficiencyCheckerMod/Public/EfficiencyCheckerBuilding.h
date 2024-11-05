// ILikeBanas

#pragma once

#include "CoreMinimal.h"

#include "Buildables/FGBuildable.h"

#include "Logic/AutoUpdateType.h"
#include "Logic/MachineStatusIncludeType.h"
#include "Logic/PlacementType.h"
#include "Logic/ProductionDetail.h"

#include "EfficiencyCheckerBuilding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams(
	FUpdateItemEvent,
	float,
	injectedInput,
	float,
	limitedThroughput,
	float,
	requiredOutput,
	const TArray<TSubclassOf<class UFGItemDescriptor>>&,
	injectedItems,
	const TArray<FProductionDetail>&,
	producers,
	const TArray<FProductionDetail>&,
	consumers,
	bool,
	overflow
	);

UCLASS(Blueprintable)
// ReSharper disable once CppClassCanBeFinal
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerBuilding : public AFGBuildable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEfficiencyCheckerBuilding();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//void CacheComponents();
	// ReSharper disable once CommentTypo
	//void CacheConnectionComponets();

	//UFGFactoryConnectionComponent* Input0 = NULL;

	//UFGFactoryConnectionComponent* Output0 = NULL;

	//UTextRenderComponent* Counter1 = NULL;
	//UTextRenderComponent* Counter2 = NULL;
	//UTextRenderComponent* LastUpdated1 = NULL;
	//UTextRenderComponent* LastUpdated2 = NULL;
	//UWidgetComponent* ItemIcon = NULL;

	bool checkTick_ = true;
	//bool checkFactoryTick_ = true;
	bool mustUpdate_ = true;

	UPROPERTY()
	class AFGBuildablePipeline* pipelineToSplit = nullptr;
	float pipelineSplitOffset = 0;

	// ReSharper disable once CommentTypo
	//UFUNCTION(BlueprintCallable, BlueprintPure = true, Category = "EfficiencyChecker")
	//static UFGFactoryConnectionComponent* GetComponentConnection(UFGFactoryConnectionComponent* component);

	UFUNCTION(BlueprintCallable, Category="EfficiencyChecker")
	virtual void SetCustomInjectedInput(bool enabled, float value);
	virtual void Server_SetCustomInjectedInput(bool enabled, float value);

	UFUNCTION(BlueprintCallable, Category="EfficiencyChecker")
	virtual void SetCustomRequiredOutput(bool enabled, float value);
	virtual void Server_SetCustomRequiredOutput(bool enabled, float value);

	UFUNCTION(BlueprintCallable, Category="EfficiencyChecker")
	virtual void SetAutoUpdateMode(EAutoUpdateType in_autoUpdateMode);
	virtual void Server_SetAutoUpdateMode(EAutoUpdateType in_autoUpdateMode);

	UFUNCTION(BlueprintCallable, Category="EfficiencyChecker")
	virtual void SetMachineStatusIncludeType
	(
		UPARAM(meta = (Bitmask,BitmaskEnum = EMachineStatusIncludeType)) int32 machineStatusIncludeType
	);
	virtual void Server_SetMachineStatusIncludeType
	(
		int32 machineStatusIncludeType
	);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "EfficiencyChecker")
	virtual void UpdateBuilding(AFGBuildable* newBuildable);
	virtual void Server_UpdateBuilding(AFGBuildable* newBuildable);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "EfficiencyChecker")
	static void UpdateBuildings(AFGBuildable* newBuildable);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "EfficiencyChecker")
	virtual void GetConnectedProduction
	(
		UPARAM(DisplayName = "Injected Input") float& out_injectedInput,
		UPARAM(DisplayName = "Limited Throughput") float& out_limitedThroughput,
		UPARAM(DisplayName = "Required Output") float& out_requiredOutput,
		UPARAM(DisplayName = "Items") TSet<TSubclassOf<UFGItemDescriptor>>& out_injectedItems,
		UPARAM(DisplayName = "Connected Buildings") TSet<AFGBuildable*>& connected,
		UPARAM(DisplayName = "Producers") TArray<FProductionDetail>& producers,
		UPARAM(DisplayName = "Consumers") TArray<FProductionDetail>& consumers,
		UPARAM(DisplayName = "Overflow") bool& out_overflow,
		UPARAM(DisplayName = "Include Production Details") bool includeProductionDetails
	);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "EfficiencyChecker")
	virtual void UpdateConnectedProduction
	(
		bool keepCustomInput,
		bool hasCustomInjectedInput,
		UPARAM(DisplayName = "Custom Injected Input") float in_customInjectedInput,
		bool keepCustomOutput,
		bool hasCustomRequiredOutput,
		UPARAM(DisplayName = "Custom Required Output") float in_customRequiredOutput,
		bool includeProductionDetails = false
	);
	virtual void Server_UpdateConnectedProduction
	(
		bool keepCustomInput,
		bool hasCustomInjectedInput,
		UPARAM(DisplayName = "Custom Injected Input") float in_customInjectedInput,
		bool keepCustomOutput,
		bool hasCustomRequiredOutput,
		UPARAM(DisplayName = "Custom Required Output") float in_customRequiredOutput,
		bool includeProductionDetails = false
	);

	UPROPERTY(BlueprintAssignable, Category = "EfficiencyChecker")
	FUpdateItemEvent OnUpdateItem;

	UFUNCTION(Category = "EfficiencyChecker", NetMulticast, Reliable)
	virtual void UpdateItem
	(
		UPARAM(DisplayName = "Injected Input") float in_injectedInput,
		UPARAM(DisplayName = "Limited Throughput") float in_limitedThroughput,
		UPARAM(DisplayName = "Required Output") float in_requiredOutput,
		UPARAM(DisplayName = "Items") const TArray<TSubclassOf<UFGItemDescriptor>>& in_injectedItems,
		UPARAM(DisplayName = "Producers") const TArray<FProductionDetail>& in_producers,
		UPARAM(DisplayName = "Consumers") const TArray<FProductionDetail>& in_consumers,
		UPARAM(DisplayName = "Overflow") bool in_overflow
	);

	UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	void AddOnDestroyBinding(AFGBuildable* buildable);

	UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	void RemoveOnDestroyBinding(AFGBuildable* buildable);

	UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	void AddOnRecipeChangedBinding(class AFGBuildableManufacturer* buildable);

	UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	void RemoveOnRecipeChangedBinding(class AFGBuildableManufacturer* buildable);

	UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	void AddOnSortRulesChangedDelegateBinding(class AFGBuildableSplitterSmart* buildable);

	UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	void RemoveOnSortRulesChangedDelegateBinding(class AFGBuildableSplitterSmart* buildable);

	UFUNCTION(BlueprintCallable, Category = "EfficiencyChecker")
	virtual void RemoveBuilding(AFGBuildable* buildable);
	virtual void Server_RemoveBuilding(AFGBuildable* buildable);

	UFUNCTION(BlueprintCallable, Category = "EfficiencyChecker")
	virtual void AddPendingBuilding(AFGBuildable* buildable);
	virtual void Server_AddPendingBuilding(AFGBuildable* buildable);

	/** The time diff from building creation time and the last update time */
	// ReSharper disable once CommentTypo
	//UPROPERTY(BlueprintReadOnly, SaveGame)
	float lastUpdated = -1;
	bool doUpdateItem = false;

	// ReSharper disable once CommentTypo
	//UPROPERTY(BlueprintReadOnly, SaveGame)
	float updateRequested = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	TArray<TSubclassOf<UFGItemDescriptor>> injectedItems;

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	float injectedInput = -1;
	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	bool customInjectedInput = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	float limitedThroughput = -1;

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	float requiredOutput = -1;
	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	bool customRequiredOutput = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	bool overflow = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated)
	EAutoUpdateType autoUpdateMode = EAutoUpdateType::AUT_USE_DEFAULT;

	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated, meta = (Bitmask, BitmaskEnum = EMachineStatusIncludeType))
	int32 machineStatusIncludeType = TO_EMachineStatusIncludeType(EMachineStatusIncludeType::MSIT_All);

	UPROPERTY(BlueprintReadOnly, SaveGame)
	// ReSharper disable once IdentifierTypo
	TSet<AFGBuildable*> connectedBuildables;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	// ReSharper disable once IdentifierTypo
	TSet<AFGBuildable*> pendingBuildables;

	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated)
	class AFGBuildablePipelineAttachment* innerPipelineAttachment = nullptr;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	EResourceForm resourceForm = EResourceForm::RF_SOLID;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	EPlacementType placementType = EPlacementType::PT_GROUND;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	TEnumAsByte<ETraceTypeQuery> traceChannel;

	FString _TAG_NAME = TEXT("EfficiencyCheckerBuilding: ");

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

protected:
	static void setPendingPotentialCallback(class AFGBuildableFactory* buildable, float potential);
	static void setPendingProductionBoostCallback(class AFGBuildableFactory* buildable, float productionBoost);

	void addOnDestroyBindings(const TSet<AFGBuildable*>& buildings);
	void removeOnDestroyBindings(const TSet<AFGBuildable*>& buildings);

	void removeOnSortRulesChangedDelegateBindings(const TSet<AFGBuildable*>& buildings);
	void addOnSortRulesChangedDelegateBindings(const TSet<AFGBuildable*>& buildings);

	void removeOnRecipeChangedBindings(const TSet<AFGBuildable*>& buildings);
	void addOnRecipeChangedBindings(const TSet<AFGBuildable*>& buildings);

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// Called every frame
	//virtual void Factory_Tick(float dt) override;
	virtual void Tick(float dt) override;
};
