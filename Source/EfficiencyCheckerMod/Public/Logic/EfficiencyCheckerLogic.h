﻿#pragma once

#include <map>

#include "ComponentFilter.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Internationalization/Regex.h"
#include "Resources/FGItemDescriptor.h"

#include "EfficiencyCheckerLogic.generated.h"

UCLASS()
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerLogic : public AActor
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="EfficiencyCheckerLogic")
	virtual void Initialize();

	UFUNCTION(BlueprintCallable, Category="EfficiencyCheckerLogic")
	virtual void Terminate();

	UFUNCTION(BlueprintCallable, Category="EfficiencyCheckerLogic")
	virtual bool IsValidBuildable(class AFGBuildable* newBuildable);

	static void collectInput
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		EResourceForm resourceForm,
		bool customInjectedInput,
		class UFGConnectionComponent* connector,
		float& out_injectedInput,
		float& out_limitedThroughput,
		std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors,
		TSet<class AFGBuildable*>& connected,
		TSet<TSubclassOf<UFGItemDescriptor>>& out_injectedItems,
		const TSet<TSubclassOf<UFGItemDescriptor>>& restrictItems,
		class AFGBuildableSubsystem* buildableSubsystem,
		int level,
		bool& overflow,
		const FString& indent,
		const time_t& timeout,
		int32 machineStatusIncludeType
	);

	static void collectOutput
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		EResourceForm resourceForm,
		class UFGConnectionComponent* connector,
		float& out_requiredOutput,
		float& out_limitedThroughput,
		std::map<AActor*, TSet<TSubclassOf<class UFGItemDescriptor>>>& seenActors,
		TSet<AFGBuildable*>& connected,
		const TSet<TSubclassOf<class UFGItemDescriptor>>& in_injectedItems,
		class AFGBuildableSubsystem* buildableSubsystem,
		int level,
		bool& overflow,
		const FString& indent,
		const time_t& timeout,
		int32 machineStatusIncludeType
	);

	static bool containsActor(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor);
	static bool actorContainsItem(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor, const TSubclassOf<UFGItemDescriptor>& item);
	static void addAllItemsToActor(std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor, const TSet<TSubclassOf<UFGItemDescriptor>>& items);

	static bool inheritsFrom(AActor* owner, const FString& className);
	static void dumpUnknownClass(const FString& indent, AActor* owner);

	static float getPipeSpeed(class AFGBuildablePipeline* pipe);

	static AEfficiencyCheckerLogic* singleton;

	TSet<class AEfficiencyCheckerBuilding*> allEfficiencyBuildings;
	// TSet<class AFGBuildableConveyorBelt*> allBelts;
	// TSet<class AFGBuildablePipeline*> allPipes;

	virtual void addEfficiencyBuilding(class AEfficiencyCheckerBuilding* actor);
	// virtual void addBelt(class AFGBuildableConveyorBelt* actor);
	// virtual void addPipe(class AFGBuildablePipeline* actor);

	UFUNCTION()
	virtual void handleBuildableConstructed(class AFGBuildable* buildable);

	// UFUNCTION()
	// virtual void removeEfficiencyBuilding(AActor* actor, EEndPlayReason::Type reason);
	// UFUNCTION()
	// virtual void removeBelt(AActor* actor, EEndPlayReason::Type reason);
	// UFUNCTION()
	// virtual void destroyBelt(AActor* actor);
	// UFUNCTION()
	// virtual void removePipe(AActor* actor, EEndPlayReason::Type reason);
	// UFUNCTION()
	// virtual void destroyPipe(AActor* actor);

	static EPipeConnectionType getConnectedPipeConnectionType(class UFGPipeConnectionComponent* component);

	static void collectUndergroundBeltsComponents
	(
		class AFGBuildableStorage* undergroundBelt,
		TSet<class UFGFactoryConnectionComponent*>& components,
		TSet<AActor*>& actors
	);

	static void collectModularLoadBalancerComponents
	(
		class AFGBuildableFactory* modularLoadBalancer,
		TSet<UFGFactoryConnectionComponent*>& components,
		TSet<AActor*>& actors
	);

	static void collectSmartSplitterComponents
	(
		class UFGConnectionComponent* connector,
		const FComponentFilter& currentFilter,
		class AFGBuildableSplitterSmart* smartSplitter,
		std::map<UFGFactoryConnectionComponent*, FComponentFilter>& connectedInputs,
		std::map<UFGFactoryConnectionComponent*, FComponentFilter>& componentOutputs,
		EFactoryConnectionDirection direction,
		const FString& indent,
		const time_t& timeout,
		bool& overflow
	);
};
