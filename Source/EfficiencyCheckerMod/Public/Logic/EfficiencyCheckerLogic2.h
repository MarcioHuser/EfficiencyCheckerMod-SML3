#pragma once

#include <functional>
#include <map>

#include "ComponentFilter.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Buildables/FGBuildableResourceExtractor.h"

#include "EfficiencyCheckerLogic2.generated.h"


UCLASS()
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerLogic2 : public AActor
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static void collectInput(class ACommonInfoSubsystem* commonInfoSubsystem, class CollectSettings& collectSettings);

	static void collectOutput(class ACommonInfoSubsystem* commonInfoSubsystem, class CollectSettings& collectSettings);

	static bool (*containsActor)(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor);
	static bool (*actorContainsItem)(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor, const TSubclassOf<UFGItemDescriptor>& item);
	// static void (*addAllItemsToActor)(std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor, const TSet<TSubclassOf<UFGItemDescriptor>>& items);
	static void addAllItemsToActor(CollectSettings& collectSettings, AActor* actor);

	static float (*getPipeSpeed)(class AFGBuildablePipeline* pipe);
	static EPipeConnectionType (*getConnectedPipeConnectionType)(class UFGPipeConnectionComponent* component);

	static void handleManufacturer
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildableManufacturer* const manufacturer,
		class CollectSettings& collectSettings,
		bool collectForInput
	);

	static void handleExtractor
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildableResourceExtractor* extractor,
		class CollectSettings& collectSettings
	);

	static void handlePortal
	(
		class AFGBuildablePortal* portal,
		CollectSettings& collectSettings
	);

	static void handleGeneratorFuel
	(
		class AFGBuildableGeneratorFuel* generatorFuel,
		CollectSettings& collectSettings
	);

	static void handlePowerBooster
	(
		class AFGBuildablePowerBooster* powerBooster,
		CollectSettings& collectSettings
	);

	static void getFactoryConnectionComponents
	(
		class AFGBuildable* buildable,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		const std::function<bool (class UFGFactoryConnectionComponent*)>& filter = [](class UFGFactoryConnectionComponent*) { return true; }
	);

	static void getPipeConnectionComponents
	(
		class AFGBuildable* buildable,
		TSet<class UFGPipeConnectionComponent*>& anyDirectionComponents,
		TSet<class UFGPipeConnectionComponent*>& inputComponents,
		TSet<class UFGPipeConnectionComponent*>& outputComponents,
		const std::function<bool (class UFGPipeConnectionComponent*)>& filter = [](class UFGPipeConnectionComponent*) { return true; }
	);

	static void handleUndergroundBeltsComponents
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildableStorage* undergroundBelt,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
	);

	static void handleContainerComponents
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildable* buildable,
		class UFGInventoryComponent* inventory,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		bool collectForInput,
		const std::function<bool (class UFGFactoryConnectionComponent*)>& filter = [](class UFGFactoryConnectionComponent*) { return true; }
	);

	static void handleTrainPlatformCargoBelt
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildableTrainPlatformCargo* trainPlatformCargo,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		bool collectForInput
	);

	static void handleTrainPlatformCargoPipe
	(
		class AFGBuildableTrainPlatformCargo* trainPlatformCargo,
		class CollectSettings& collectSettings,
		TSet<class UFGPipeConnectionComponent*>& inputComponents,
		TSet<class UFGPipeConnectionComponent*>& outputComponents,
		bool collectForInput
	);

	static void handleStorageTeleporter
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildable* storageTeleporter,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		bool collectForInput
	);

	static void handleModularLoadBalancerComponents
	(
		AFGBuildableFactory* modularLoadBalancerGroupLeader,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		bool collectForInput
	);

	static void handleSmartSplitterComponents
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildableSplitterSmart* smartSplitter,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		bool collectForInput
	);

	static void handleFluidIntegrant
	(
		class IFGFluidIntegrantInterface* fluidIntegrant,
		class CollectSettings& collectSettings,
		TSet<class UFGPipeConnectionComponent*>& anyDirectionComponents,
		TSet<class UFGPipeConnectionComponent*>& inputComponents,
		TSet<class UFGPipeConnectionComponent*>& outputComponents
	);

	static void handleDroneStation
	(
		class ACommonInfoSubsystem* commonInfoSubsystem,
		class AFGBuildableDroneStation* droneStation,
		class CollectSettings& collectSettings,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		bool collectForInput
	);

	static UFGPipeConnectionComponent*
	getFirstItem(const TSet<UFGPipeConnectionComponent*>& connections)
	{
		UFGPipeConnectionComponent* firstItem = nullptr;

		if (!connections.IsEmpty())
		{
			firstItem = connections[connections.begin().GetId()];
		}

		return firstItem;
	}

	static UFGFactoryConnectionComponent* getComponentByIndex(std::map<UFGFactoryConnectionComponent*, FComponentFilter> componentsMap, int index);
};
