#pragma once

#include <functional>
#include <map>

#include "ComponentFilter.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Internationalization/Regex.h"

#include "EfficiencyCheckerLogic2.generated.h"


UCLASS()
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerLogic2 : public AActor
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static void collectInput(class ICollectSettings& collectSettings);

	static void collectOutput(class ICollectSettings& collectSettings);

	static void handleManufacturer
	(
		class AFGBuildableManufacturer* const manufacturer,
		class ICollectSettings& collectSettings,
		bool collectInjectedInput,
		bool collectRequiredOutput
	);

	static void handleExtractor(AFGBuildableResourceExtractor* extractor, class ICollectSettings& collectSettings);

	static void handleFactoryComponents
	(
		class AFGBuildable* buildable,
		EFactoryConnectionConnector connectorType,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		const std::function<bool (class UFGFactoryConnectionComponent*)>& filter = [](class UFGFactoryConnectionComponent*) { return true; }
	);

	static void handleUndergroundBeltsComponents
	(
		class AFGBuildableStorage* undergroundBelt,
		class ICollectSettings& collectSettings,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
	);

	static void handleContainerComponents
	(
		class AFGBuildable* buildable,
		EFactoryConnectionConnector connectorType,
		class UFGInventoryComponent* inventory,
		class ICollectSettings& collectSettings,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
		const std::function<bool (class UFGFactoryConnectionComponent*)>& filter = [](class UFGFactoryConnectionComponent*) { return true; }
	);

	static void handleTrainPlatformCargo
	(
		class AFGBuildableTrainPlatformCargo* trainPlatformCargo,
		EFactoryConnectionConnector connectorType,
		class ICollectSettings& collectSettings,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
	);

	static void handleStorageTeleporter
	(
		class AFGBuildable* storageTeleporter,
		class ICollectSettings& collectSettings,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
	);

	static void handleModularLoadBalancerComponents
	(
		AFGBuildableFactory* modularLoadBalancer,
		class ICollectSettings& collectSettings,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
	);

	static void hanbleSmartSplitterComponents
	(
		class AFGBuildableSplitterSmart* smartSplitter,
		class ICollectSettings& collectSettings,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
		TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
	);
};
