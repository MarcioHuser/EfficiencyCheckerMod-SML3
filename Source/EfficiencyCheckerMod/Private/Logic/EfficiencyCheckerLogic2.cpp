#include "Logic/EfficiencyCheckerLogic2.h"
#include "Util/ECMOptimize.h"
#include "EfficiencyCheckerBuilding.h"
#include "FGBuildableSubsystem.h"
#include "FGRailroadSubsystem.h"
#include "FGRailroadTimeTable.h"
#include "FGTrain.h"
#include "FGTrainStationIdentifier.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableGeneratorNuclear.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableTrainPlatform.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "Kismet/GameplayStatics.h"
#include "Logic/CollectSettings.h"
#include "Logic/EfficiencyCheckerLogic.h"
#include "Patching/NativeHookManager.h"
#include "Resources/FGEquipmentDescriptor.h"
#include "Util/ECMLogging.h"
#include "Util/EfficiencyCheckerConfiguration.h"

#include <set>

#include "Buildables/FGBuildableFactorySimpleProducer.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Reflection/BlueprintReflectionLibrary.h"
#include "Subsystems/CommonInfoSubsystem.h"

#ifndef OPTIMIZE
#pragma optimize("", off )
#endif

bool (*AEfficiencyCheckerLogic2::containsActor)(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor) = &AEfficiencyCheckerLogic::containsActor;
bool (*AEfficiencyCheckerLogic2::actorContainsItem)
	(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor, const TSubclassOf<UFGItemDescriptor>& item) = &
	AEfficiencyCheckerLogic::actorContainsItem;
// void (*AEfficiencyCheckerLogic2::addAllItemsToActor)
// 	(std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor, const TSet<TSubclassOf<UFGItemDescriptor>>& items) = &
// 	AEfficiencyCheckerLogic::addAllItemsToActor;

float (*AEfficiencyCheckerLogic2::getPipeSpeed)(class AFGBuildablePipeline* pipe) = &AEfficiencyCheckerLogic::getPipeSpeed;
EPipeConnectionType (*AEfficiencyCheckerLogic2::getConnectedPipeConnectionType)
	(class UFGPipeConnectionComponent* component) = &AEfficiencyCheckerLogic::getConnectedPipeConnectionType;

void AEfficiencyCheckerLogic2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
}

void lodAttachmentConnector(CollectSettings& collectSettings, UFGConnectionComponent* connection)
{
	EC_LOG_Display_Condition(
		/**getTimeStamp(),*/
		*collectSettings.GetIndent(),
		TEXT("    Collecting from "),
		*connection->GetName()
		);
}

void AEfficiencyCheckerLogic2::collectInput(CollectSettings& collectSettings)
{
	auto commonInfoSubsystem = ACommonInfoSubsystem::Get();

	if(!commonInfoSubsystem)
	{
		return;
	}

	for (;;)
	{
		if (!collectSettings.GetConnector())
		{
			return;
		}

		auto owner = collectSettings.GetConnector()->GetOwner();

		if (!owner || collectSettings.GetSeenActors().contains(owner))
		{
			return;
		}

		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout!"));

			collectSettings.SetOverflow(true);
			return;
		}

		const auto fullClassName = GetPathNameSafe(owner->GetClass());

		if (collectSettings.GetLevel() > 100)
		{
			EC_LOG_Error_Condition(
				FUNCTIONSTR TEXT(": level is too deep: "),
				collectSettings.GetLevel(),
				TEXT("; "),
				*owner->GetName(),
				TEXT(" / "),
				*fullClassName
				);

			collectSettings.SetOverflow(true);

			return;
		}

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*collectSettings.GetIndent(),
			TEXT("collectInput at level "),
			collectSettings.GetLevel(),
			TEXT(": "),
			*owner->GetName(),
			TEXT(" / "),
			*fullClassName,
			TEXT(" (connection "),
			*collectSettings.GetConnector()->GetName(),
			TEXT(")")
			);

		collectSettings.GetSeenActors()[owner];

		{
			const auto manufacturer = Cast<AFGBuildableManufacturer>(owner);

			if (manufacturer)
			{
				handleManufacturer(
					manufacturer,
					collectSettings,
					true
					);

				return;
			}
		}

		{
			const auto extractor = Cast<AFGBuildableResourceExtractor>(owner);
			if (extractor)
			{
				handleExtractor(
					extractor,
					collectSettings
					);

				return;
			}
		}

		if (collectSettings.GetResourceForm() == EResourceForm::RF_SOLID)
		{
			if (const auto conveyor = Cast<AFGBuildableConveyorBase>(owner))
			{
				// The initial limit for a belt is its own speed
				collectSettings.GetConnected().Add(conveyor);

				collectSettings.SetConnector(conveyor->GetConnection0()->GetConnection());

				collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), conveyor->GetSpeed() / 2));

				EC_LOG_Display_Condition(*collectSettings.GetIndent(), *conveyor->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" items/minute"));

				continue;
			}

			if (commonInfoSubsystem->baseCounterLimiterClass && owner->IsA(commonInfoSubsystem->baseCounterLimiterClass))
			{
				auto buildable = Cast<AFGBuildable>(owner);

				addAllItemsToActor(collectSettings, buildable);

				collectSettings.GetConnected().Add(buildable);

				std::map<UFGFactoryConnectionComponent*, FComponentFilter> inputComponents, outputComponents;

				getFactoryConnectionComponents(buildable, inputComponents, outputComponents);

				if (inputComponents.empty())
				{
					// Nowhere to go
					return;
				}

				collectSettings.SetConnector(inputComponents.begin()->first->GetConnection());

				auto limitRate = FReflectionHelper::GetPropertyValue<FFloatProperty>(buildable, TEXT("mPerMinuteLimitRate"));

				if (limitRate >= 0)
				{
					collectSettings.SetLimitedThroughput(
						FMath::Min(
							collectSettings.GetLimitedThroughput(),
							limitRate
							)
						);
				}

				EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" items/minute"));

				continue;
			}

			AFGBuildable* buildable = nullptr;

			std::map<UFGFactoryConnectionComponent*, FComponentFilter> inputComponents, outputComponents;

			if (!buildable &&
				(commonInfoSubsystem->baseUndergroundSplitterInputClass && owner->IsA(commonInfoSubsystem->baseUndergroundSplitterInputClass) ||
					commonInfoSubsystem->baseUndergroundSplitterOutputClass && owner->IsA(commonInfoSubsystem->baseUndergroundSplitterOutputClass)))
			{
				if (auto undergroundBelt = Cast<AFGBuildableStorage>(owner))
				{
					buildable = undergroundBelt;

					handleUndergroundBeltsComponents(undergroundBelt, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!AEfficiencyCheckerConfiguration::configuration.ignoreStorageTeleporter &&
				!buildable && commonInfoSubsystem->baseStorageTeleporterClass && owner->IsA(commonInfoSubsystem->baseStorageTeleporterClass))
			{
				if (auto storageTeleporter = Cast<AFGBuildableFactory>(owner))
				{
					buildable = storageTeleporter;

					handleStorageTeleporter(storageTeleporter, collectSettings, inputComponents, outputComponents, true);
				}
			}

			if (!buildable && commonInfoSubsystem->baseModularLoadBalancerClass && owner->IsA(commonInfoSubsystem->baseModularLoadBalancerClass))
			{
				if (auto modularLoadBalancer = FReflectionHelper::GetObjectPropertyValue<AFGBuildableFactory>(owner, TEXT("GroupLeader")))
				{
					buildable = modularLoadBalancer;

					handleModularLoadBalancerComponents(modularLoadBalancer, collectSettings, inputComponents, outputComponents, true);
				}
			}

			if (!buildable)
			{
				if (auto splitterSmart = Cast<AFGBuildableSplitterSmart>(owner))
				{
					buildable = splitterSmart;

					handleSmartSplitterComponents(splitterSmart, collectSettings, inputComponents, outputComponents, true);
				}
			}

			if (!buildable)
			{
				buildable = Cast<AFGBuildableConveyorAttachment>(owner);
				if (buildable)
				{
					getFactoryConnectionComponents(buildable, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				if (auto storageContainer = Cast<AFGBuildableStorage>(owner))
				{
					buildable = storageContainer;

					handleContainerComponents(
						storageContainer,
						storageContainer->GetStorageInventory(),
						collectSettings,
						inputComponents,
						outputComponents,
						true
						);
				}
			}

			if (!buildable)
			{
				if (auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner))
				{
					buildable = cargoPlatform;

					handleTrainPlatformCargoBelt(cargoPlatform, collectSettings, inputComponents, outputComponents, true);
				}
			}

			if (!buildable)
			{
				if (auto dockingStation = Cast<AFGBuildableDockingStation>(owner))
				{
					buildable = dockingStation;

					handleContainerComponents(
						dockingStation,
						dockingStation->GetInventory(),
						collectSettings,
						inputComponents,
						outputComponents,
						true,
						[](class UFGFactoryConnectionComponent* component)
						{
							return !component->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase);
						}
						);
				}
			}

			if (buildable)
			{
				// Update filter
				collectSettings.SetCurrentFilter(
					FComponentFilter::combineFilters(
						collectSettings.GetCurrentFilter(),
						outputComponents[Cast<UFGFactoryConnectionComponent>(collectSettings.GetConnector())]
						)
					);

				if (inputComponents.size() == 1 && outputComponents.size() == 1)
				{
					collectSettings.GetConnected().Add(buildable);

					collectSettings.SetConnector(inputComponents.begin()->first->GetConnection());

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" skipped"));

					collectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), inputComponents.begin()->second));

					// Check if anything can still POSSIBLY flow through
					if (collectSettings.GetCurrentFilter().allowedFiltered && collectSettings.GetCurrentFilter().allowedItems.IsEmpty())
					{
						// Is filtered and has no "pass-through" item. Stop crawling
						return;
					}

					continue;
				}

				if (inputComponents.size() == 0)
				{
					// Nothing is being inputed. Bail
					EC_LOG_Error_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" has no input"));
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					for (const auto& connectionEntry : inputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.first;

						if (connection == collectSettings.GetConnector())
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						// if (dockingStation && connection->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase))
						// {
						// 	continue;
						// }

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(collectSettings.GetLimitedThroughput(), true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), connectionEntry.second), true);

						collectInput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (firstConnection)
						{
							limitedThroughput = tempCollectSettings.GetLimitedThroughput();
							firstConnection = false;
						}
						else
						{
							limitedThroughput += tempCollectSettings.GetLimitedThroughput();
						}
					}

					collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), limitedThroughput));

					for (const auto& connectionEntry : outputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.first;

						if (connection == collectSettings.GetConnector())
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(0, true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), connectionEntry.second), true);
						tempCollectSettings.SetInjectedInputPtr(nullptr);
						tempCollectSettings.SetRequiredOutputPtr(nullptr);
						tempCollectSettings.SetSeenActors(collectSettings.GetSeenActors(), true);
						// tempCollectSettings.SetInjectedItems(tempCollectSettings.GetCurrentFilter().allowedItems, true);

						collectOutput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (tempCollectSettings.GetRequiredOutputTotal() > 0)
						{
							EC_LOG_Display_Condition(*tempCollectSettings.GetIndent(), TEXT("Discounting "), tempCollectSettings.GetRequiredOutputTotal(), TEXT(" items/minute"));

							for (const auto& entry : tempCollectSettings.GetRequiredOutput())
							{
								if (!collectSettings.GetCurrentFilter().itemIsAllowed(entry.first))
								{
									continue;
								}

								collectSettings.GetInjectedInput()[entry.first] -= entry.second;
							}
						}
					}
				}

				collectSettings.GetConnected().Add(buildable);

				return;
			}
		}

		if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
		{
			TSet<class UFGPipeConnectionComponent*> anyDirectionComponents, inputComponents, outputComponents;

			auto pipeline = Cast<AFGBuildablePipeline>(owner);
			auto pipeConnection = Cast<UFGPipeConnectionComponent>(collectSettings.GetConnector());

			AFGBuildable* buildable = nullptr;

			{
				if (auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(owner))
				{
					buildable = Cast<AFGBuildable>(fluidIntegrant);

					handleFluidIntegrant(
						fluidIntegrant,
						collectSettings,
						anyDirectionComponents,
						inputComponents,
						outputComponents
						);
				}
			}

			if (!buildable)
			{
				if (auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner))
				{
					buildable = cargoPlatform;

					handleTrainPlatformCargoPipe(
						cargoPlatform,
						collectSettings,
						inputComponents,
						outputComponents,
						true
						);
				}
			}

			if (buildable)
			{
				auto allConnections = anyDirectionComponents.Union(inputComponents).Union(outputComponents);
				auto firstAnyDirection = getFirstItem(allConnections);

				if (allConnections.IsEmpty())
				{
					// No more connections. Bail
					EC_LOG_Error_Condition(*collectSettings.GetIndent(), *owner->GetName(), TEXT(" has no other connection"));
				}
				else if (allConnections.Num() == 1 &&
					(!anyDirectionComponents.IsEmpty() ||
						pipeConnection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER &&
						firstAnyDirection->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER)
					)
				{
					// Just pass-through
					collectSettings.GetConnected().Add(buildable);

					collectSettings.SetConnector(firstAnyDirection->GetConnection());

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" skipped"));

					continue;
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					auto firstActor = collectSettings.GetSeenActors().size() == 1;

					for (auto connection : anyDirectionComponents.Union(inputComponents))
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						if (connection == pipeConnection && !firstActor)
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(collectSettings.GetLimitedThroughput(), true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);

						collectInput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (pipeline)
						{
							collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), tempCollectSettings.GetLimitedThroughput()));
						}
						else if (firstConnection)
						{
							limitedThroughput = tempCollectSettings.GetLimitedThroughput();
							firstConnection = false;
						}
						else
						{
							limitedThroughput += tempCollectSettings.GetLimitedThroughput();
						}
					}

					if (!pipeline && !firstConnection)
					{
						collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), limitedThroughput));
					}

					for (auto connection : outputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						if (connection->GetPipeConnectionType() != EPipeConnectionType::PCT_CONSUMER)
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(0, true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetInjectedInputPtr(nullptr);
						tempCollectSettings.SetRequiredOutputPtr(nullptr);
						tempCollectSettings.SetSeenActors(collectSettings.GetSeenActors(), true);

						collectOutput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (tempCollectSettings.GetInjectedInputTotal() > 0)
						{
							EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Discounting "), tempCollectSettings.GetInjectedInputTotal(), TEXT(" m³/minute"));

							if (!collectSettings.GetCustomInjectedInput() && !collectSettings.GetInjectedInput().empty())
							{
								auto item = collectSettings.GetInjectedInput().begin()->first;

								collectSettings.GetInjectedInput()[item] -= tempCollectSettings.GetInjectedInput()[item];
							}
						}
					}

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *owner->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" m³/minute"));
				}

				collectSettings.GetConnected().Add(buildable);

				return;
			}
		}

		{
			const auto nuclearGenerator = Cast<AFGBuildableGeneratorNuclear>(owner);
			if (nuclearGenerator &&
				(nuclearGenerator->HasPower() || Has_EMachineStatusIncludeType(collectSettings. GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!nuclearGenerator->IsProductionPaused() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Paused)))
			{
				for (auto item : commonInfoSubsystem->nuclearWasteItemDescriptors)
				{
					// collectSettings.GetInjectedItems().Add(item);

					collectSettings.GetInjectedInput()[item] += 0.2;
				}

				collectSettings.GetConnected().Add(nuclearGenerator);

				return;
			}
		}

		if (auto itemProducer = Cast<AFGBuildableFactorySimpleProducer>(owner))
		{
			auto itemType = itemProducer->mItemType;
			auto timeToProduceItem = itemProducer->mTimeToProduceItem;

			if (timeToProduceItem && itemType)
			{
				// collectSettings.GetInjectedItems().Add(itemType);

				collectSettings.GetInjectedInput()[itemType] += 60 / timeToProduceItem;
			}

			collectSettings.GetConnected().Add(Cast<AFGBuildable>(owner));

			return;
		}

		return;
	}
}

void AEfficiencyCheckerLogic2::collectOutput(CollectSettings& collectSettings)
{
	auto commonInfoSubsystem = ACommonInfoSubsystem::Get();
	
	if(!commonInfoSubsystem)
	{
		return;
	}

	// TSet<TSubclassOf<UFGItemDescriptor>> injectedItems;

	// for (const auto& entry : collectSettings.GetInjectedInput())
	// {
	// 	injectedItems.Add(entry.first);
	// }

	for (;;)
	{
		if (!collectSettings.GetConnector())
		{
			return;
		}

		auto owner = collectSettings.GetConnector()->GetOwner();

		if (!owner)
		{
			return;
		}

		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout!"));

			collectSettings.SetOverflow(true);
			return;
		}

		const auto fullClassName = GetPathNameSafe(owner->GetClass());

		if (collectSettings.GetLevel() > 100)
		{
			EC_LOG_Error_Condition(
				FUNCTIONSTR TEXT(": level is too deep: "),
				collectSettings.GetLevel(),
				TEXT("; "),
				*owner->GetName(),
				TEXT(" / "),
				*fullClassName
				);

			collectSettings.SetOverflow(true);

			return;
		}

		if (!collectSettings.GetInjectedInput().empty())
		{
			bool unusedItems = false;

			for (auto item : collectSettings.GetInjectedInput())
			{
				if (!actorContainsItem(collectSettings.GetSeenActors(), owner, item.first))
				{
					unusedItems = true;
					break;
				}
			}

			if (!unusedItems)
			{
				return;
			}
		}
		else
		{
			if (containsActor(collectSettings.GetSeenActors(), owner))
			{
				return;
			}
		}

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*collectSettings.GetIndent(),
			TEXT("collectOutput at level "),
			collectSettings.GetLevel(),
			TEXT(": "),
			*owner->GetName(),
			TEXT(" / "),
			*fullClassName,
			TEXT(" (connection "),
			*collectSettings.GetConnector()->GetName(),
			TEXT(")")
			);

		{
			const auto manufacturer = Cast<AFGBuildableManufacturer>(owner);

			if (manufacturer)
			{
				handleManufacturer(
					manufacturer,
					collectSettings,
					false
					);

				return;
			}
		}

		if (collectSettings.GetResourceForm() == EResourceForm::RF_SOLID)
		{
			if (const auto conveyor = Cast<AFGBuildableConveyorBase>(owner))
			{
				addAllItemsToActor(collectSettings, conveyor);

				collectSettings.GetConnected().Add(conveyor);

				collectSettings.SetConnector(conveyor->GetConnection1()->GetConnection());

				collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), conveyor->GetSpeed() / 2));

				EC_LOG_Display_Condition(*collectSettings.GetIndent(), *conveyor->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" items/minute"));

				continue;
			}

			if (commonInfoSubsystem->baseCounterLimiterClass && owner->IsA(commonInfoSubsystem->baseCounterLimiterClass))
			{
				auto buildable = Cast<AFGBuildable>(owner);

				addAllItemsToActor(collectSettings, buildable);

				collectSettings.GetConnected().Add(buildable);

				std::map<UFGFactoryConnectionComponent*, FComponentFilter> inputComponents, outputComponents;

				getFactoryConnectionComponents(buildable, inputComponents, outputComponents);

				if (outputComponents.empty())
				{
					// Nowhere to go
					return;
				}

				collectSettings.SetConnector(outputComponents.begin()->first->GetConnection());

				auto limitRate = FReflectionHelper::GetPropertyValue<FFloatProperty>(buildable, TEXT("mPerMinuteLimitRate"));

				if (limitRate >= 0)
				{
					collectSettings.SetLimitedThroughput(
						FMath::Min(
							collectSettings.GetLimitedThroughput(),
							limitRate
							)
						);
				}

				EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" items/minute"));

				continue;
			}

			AFGBuildable* buildable = nullptr;

			std::map<UFGFactoryConnectionComponent*, FComponentFilter> inputComponents, outputComponents;

			if (!buildable &&
				(commonInfoSubsystem->baseUndergroundSplitterInputClass && owner->IsA(commonInfoSubsystem->baseUndergroundSplitterInputClass) ||
					commonInfoSubsystem->baseUndergroundSplitterOutputClass && owner->IsA(commonInfoSubsystem->baseUndergroundSplitterOutputClass)))
			{
				if (auto undergroundBelt = Cast<AFGBuildableStorage>(owner))
				{
					buildable = undergroundBelt;

					handleUndergroundBeltsComponents(undergroundBelt, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!AEfficiencyCheckerConfiguration::configuration.ignoreStorageTeleporter &&
				!buildable && commonInfoSubsystem->baseStorageTeleporterClass && owner->IsA(commonInfoSubsystem->baseStorageTeleporterClass))
			{
				auto storageTeleporter = Cast<AFGBuildableFactory>(owner);
				buildable = storageTeleporter;

				if (storageTeleporter)
				{
					handleStorageTeleporter(storageTeleporter, collectSettings, inputComponents, outputComponents, false);
				}
			}

			if (!buildable && commonInfoSubsystem->baseModularLoadBalancerClass && owner->IsA(commonInfoSubsystem->baseModularLoadBalancerClass))
			{
				if (auto modularLoadBalancerGroupLeader = FReflectionHelper::GetObjectPropertyValue<AFGBuildableFactory>(owner, TEXT("GroupLeader")))
				{
					buildable = modularLoadBalancerGroupLeader;

					handleModularLoadBalancerComponents(modularLoadBalancerGroupLeader, collectSettings, inputComponents, outputComponents, false);
				}
			}

			if (!buildable)
			{
				if (auto splitterSmart = Cast<AFGBuildableSplitterSmart>(owner))
				{
					buildable = splitterSmart;

					handleSmartSplitterComponents(splitterSmart, collectSettings, inputComponents, outputComponents, false);
				}
			}

			if (!buildable)
			{
				buildable = Cast<AFGBuildableConveyorAttachment>(owner);
				if (buildable)
				{
					getFactoryConnectionComponents(buildable, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				if (auto storageContainer = Cast<AFGBuildableStorage>(owner))
				{
					buildable = storageContainer;

					handleContainerComponents(
						storageContainer,
						storageContainer->GetStorageInventory(),
						collectSettings,
						inputComponents,
						outputComponents,
						true
						);
				}
			}

			if (!buildable)
			{
				if (auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner))
				{
					buildable = cargoPlatform;

					handleTrainPlatformCargoBelt(cargoPlatform, collectSettings, inputComponents, outputComponents, false);
				}
			}

			if (!buildable)
			{
				if (auto dockingStation = Cast<AFGBuildableDockingStation>(owner))
				{
					buildable = dockingStation;

					handleContainerComponents(
						dockingStation,
						dockingStation->GetInventory(),
						collectSettings,
						inputComponents,
						outputComponents,
						false,
						[](class UFGFactoryConnectionComponent* component)
						{
							return !component->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase);
						}
						);
				}
			}

			if (buildable)
			{
				addAllItemsToActor(collectSettings, buildable);

				if (inputComponents.size() == 1 && outputComponents.size() == 1)
				{
					collectSettings.GetConnected().Add(buildable);

					collectSettings.SetConnector(outputComponents.begin()->first->GetConnection());

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" skipped"));

					// injectedItems = outputComponents.begin()->second.filterItems(injectedItems);

					collectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), outputComponents.begin()->second));

					// Check if anything can still POSSIBLY flow through
					if (collectSettings.GetCurrentFilter().allowedFiltered && collectSettings.GetCurrentFilter().allowedItems.IsEmpty())
					{
						// Is filtered and has no "pass-through" item. Stop crawling
						return;
					}

					continue;
				}

				if (outputComponents.size() == 0)
				{
					// Nothing is being outputed. Bail
					EC_LOG_Error_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" has no output"));
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					for (const auto& connectionEntry : outputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.first;

						if (connection == collectSettings.GetConnector())
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(collectSettings.GetLimitedThroughput(), true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), connectionEntry.second), true);

						collectOutput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (firstConnection)
						{
							limitedThroughput = tempCollectSettings.GetLimitedThroughput();
							firstConnection = false;
						}
						else
						{
							limitedThroughput += tempCollectSettings.GetLimitedThroughput();
						}
					}

					collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), limitedThroughput));

					for (const auto& connectionEntry : inputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.first;

						if (connection == collectSettings.GetConnector())
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(0, true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), connectionEntry.second), true);
						tempCollectSettings.SetInjectedInputPtr(nullptr);
						tempCollectSettings.SetRequiredOutputPtr(nullptr);
						tempCollectSettings.SetSeenActors(collectSettings.GetSeenActors(), true);
						// tempCollectSettings.SetInjectedItems(injectedItems, true);

						collectInput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (tempCollectSettings.GetInjectedInputTotal() > 0)
						{
							EC_LOG_Display_Condition(*tempCollectSettings.GetIndent(), TEXT("Discounting "), tempCollectSettings.GetInjectedInputTotal(), TEXT(" items/minute"));

							for (const auto& entry : tempCollectSettings.GetInjectedInput())
							{
								if (!collectSettings.GetCurrentFilter().itemIsAllowed(entry.first))
								{
									continue;
								}

								collectSettings.GetRequiredOutput()[entry.first] -= entry.second;
							}
						}
					}
				}

				return;
			}
		}

		if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
		{
			TSet<class UFGPipeConnectionComponent*> anyDirectionComponents, inputComponents, outputComponents;

			auto pipeline = Cast<AFGBuildablePipeline>(owner);
			auto pipeConnection = Cast<UFGPipeConnectionComponent>(collectSettings.GetConnector());

			AFGBuildable* buildable = nullptr;

			{
				if (auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(owner))
				{
					buildable = Cast<AFGBuildable>(fluidIntegrant);

					addAllItemsToActor(collectSettings, buildable);

					handleFluidIntegrant(
						fluidIntegrant,
						collectSettings,
						anyDirectionComponents,
						inputComponents,
						outputComponents
						);
				}
			}

			if (!buildable)
			{
				if (auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner))
				{
					buildable = cargoPlatform;

					addAllItemsToActor(collectSettings, buildable);

					handleTrainPlatformCargoPipe(
						cargoPlatform,
						collectSettings,
						inputComponents,
						outputComponents,
						false
						);
				}
			}

			if (buildable)
			{
				auto allConnections = anyDirectionComponents.Union(inputComponents).Union(outputComponents);
				auto firstAnyDirection = getFirstItem(allConnections);

				if (allConnections.IsEmpty())
				{
					// No more connections. Bail
					EC_LOG_Error_Condition(*collectSettings.GetIndent(), *owner->GetName(), TEXT(" has no other connection"));
				}
				else if (allConnections.Num() == 1 &&
					(!anyDirectionComponents.IsEmpty() ||
						pipeConnection->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER &&
						firstAnyDirection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER)
					)
				{
					// Just pass-through
					collectSettings.GetConnected().Add(buildable);

					collectSettings.SetConnector(firstAnyDirection->GetConnection());

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" skipped"));

					continue;
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					auto firstActor = collectSettings.GetSeenActors().size() == 1;

					for (auto connection : anyDirectionComponents.Union(outputComponents))
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						if (connection == pipeConnection && !firstActor)
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(collectSettings.GetLimitedThroughput(), true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);

						collectOutput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (pipeline)
						{
							collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), tempCollectSettings.GetLimitedThroughput()));
						}
						else if (firstConnection)
						{
							limitedThroughput = tempCollectSettings.GetLimitedThroughput();
							firstConnection = false;
						}
						else
						{
							limitedThroughput += tempCollectSettings.GetLimitedThroughput();
						}
					}

					if (!pipeline && !firstConnection)
					{
						collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), limitedThroughput));
					}

					for (auto connection : inputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						if (connection->GetPipeConnectionType() != EPipeConnectionType::PCT_PRODUCER)
						{
							continue;
						}

						lodAttachmentConnector(collectSettings, connection);

						CollectSettings tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(0, true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("        "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetInjectedInputPtr(nullptr);
						tempCollectSettings.SetRequiredOutputPtr(nullptr);
						tempCollectSettings.SetSeenActors(collectSettings.GetSeenActors(), true);

						collectInput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (tempCollectSettings.GetRequiredOutputTotal() > 0)
						{
							EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Discounting "), tempCollectSettings.GetRequiredOutputTotal(), TEXT(" m³/minute"));

							if (!collectSettings.GetCustomRequiredOutput() && !collectSettings.GetRequiredOutput().empty())
							{
								auto item = *collectSettings.GetRequiredOutput().begin()->first;

								collectSettings.GetRequiredOutput()[item] -= tempCollectSettings.GetRequiredOutput()[item];
							}
						}
					}

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *owner->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" m³/minute"));
				}

				collectSettings.GetConnected().Add(buildable);

				return;
			}
		}

		{
			const auto generator = Cast<AFGBuildableGeneratorFuel>(owner);
			if (generator)
			{
				handleGeneratorFuel(generator, collectSettings);

				return;
			}
		}

		addAllItemsToActor(collectSettings, owner);

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			AEfficiencyCheckerLogic::dumpUnknownClass(collectSettings.GetIndent(), owner);
		}

		return;
	}
}

void AEfficiencyCheckerLogic2::handleManufacturer
(
	class AFGBuildableManufacturer* const manufacturer,
	CollectSettings& collectSettings,
	bool collectForInput
)
{
	if ((manufacturer->HasPower() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Unpowered)) &&
		(!manufacturer->IsProductionPaused() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Paused)))
	{
		const auto recipeClass = manufacturer->GetCurrentRecipe();

		collectSettings.GetConnected().Add(manufacturer);

		if (recipeClass)
		{
			if (collectForInput)
			{
				auto products = UFGRecipe::GetProducts(recipeClass)
					.FilterByPredicate(
						[&collectSettings](const FItemAmount& item)
						{
							return UFGItemDescriptor::GetForm(item.ItemClass) == collectSettings.GetResourceForm();
						}
						);

				if (!products.IsEmpty())
				{
					int outputIndex = 0;

					if (products.Num() > 1)
					{
						TArray<FString> names;

						auto isBelt = Cast<UFGFactoryConnectionComponent>(collectSettings.GetConnector()) != nullptr;
						auto isPipe = Cast<UFGPipeConnectionComponent>(collectSettings.GetConnector()) != nullptr;

						TInlineComponentArray<UFGConnectionComponent*> components;
						manufacturer->GetComponents(components);

						for (auto component : components)
						{
							if (isBelt)
							{
								auto factoryConnectionComponent = Cast<UFGFactoryConnectionComponent>(component);
								if (factoryConnectionComponent && factoryConnectionComponent->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
								{
									names.Add(component->GetName());

									continue;
								}
							}

							if (isPipe)
							{
								auto pipeConnectionComponent = Cast<UFGPipeConnectionComponent>(component);
								if (pipeConnectionComponent && pipeConnectionComponent->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER)
								{
									names.Add(component->GetName());

									continue;
								}
							}
						}

						names.Sort(
							[](const FString& x, const FString& y)
							{
								auto index1 = -1;
								auto index2 = -1;

								FRegexMatcher m1(AEfficiencyCheckerLogic::indexPattern, x);
								if (m1.FindNext())
								{
									index1 = FCString::Atoi(*m1.GetCaptureGroup(1)) - 1;
								}

								FRegexMatcher m2(AEfficiencyCheckerLogic::indexPattern, y);
								if (m2.FindNext())
								{
									index2 = FCString::Atoi(*m2.GetCaptureGroup(1)) - 1;
								}

								auto order = index1 - index2;

								if (!order)
								{
									order = x.Compare(y, ESearchCase::IgnoreCase);
								}

								return order < 0;
							}
							);

						outputIndex = names.Find(collectSettings.GetConnector()->GetName());
					}

					auto item = products[FMath::Max(outputIndex, 0)];

					if (!collectSettings.GetCurrentFilter().itemIsAllowed(item.ItemClass))
					{
						return;
					}

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("    Item amount = "), item.Amount);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("    Current potential = "), manufacturer->GetCurrentPotential());
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("    Pending potential = "), manufacturer->GetPendingPotential());
					EC_LOG_Display_Condition(
						*collectSettings.GetIndent(),
						TEXT("    Production cycle time = "),
						manufacturer->CalcProductionCycleTimeForPotential(manufacturer->GetPendingPotential())
						);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("    Recipe duration = "), UFGRecipe::GetManufacturingDuration(recipeClass));

					float itemAmountPerMinute = item.Amount * manufacturer->GetPendingPotential() * 60 /
						manufacturer->GetDefaultProductionCycleTime();

					if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
					{
						itemAmountPerMinute /= 1000;
					}

					EC_LOG_Display_Condition(
						/**getTimeStamp(),*/
						*collectSettings.GetIndent(),
						TEXT("    "),
						*manufacturer->GetName(),
						TEXT(" produces "),
						itemAmountPerMinute,
						TEXT(" "),
						*UFGItemDescriptor::GetItemName(item.ItemClass).ToString(),
						TEXT("/minute")
						);

					collectSettings.GetInjectedInput()[item.ItemClass];

					if (!collectSettings.GetCustomInjectedInput())
					{
						collectSettings.GetInjectedInput()[item.ItemClass] += itemAmountPerMinute;
					}
				}
			}
			else
			{
				auto ingredients = UFGRecipe::GetIngredients(recipeClass);

				for (auto item : ingredients)
				{
					auto itemForm = UFGItemDescriptor::GetForm(item.ItemClass);

					if (itemForm == EResourceForm::RF_SOLID && collectSettings.GetResourceForm() != EResourceForm::RF_SOLID ||
						(itemForm == EResourceForm::RF_LIQUID || itemForm == EResourceForm::RF_GAS) &&
						collectSettings.GetResourceForm() != EResourceForm::RF_LIQUID && collectSettings.GetResourceForm() != EResourceForm::RF_GAS)
					{
						continue;
					}

					if (!collectSettings.GetInjectedInput().empty() && !collectSettings.GetInjectedInput().contains(item.ItemClass))
					{
						// Item is not being inject by anyone. Ignore this
						continue;
						// EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Item is not being inject"));
					}

					if (!collectSettings.GetCurrentFilter().itemIsAllowed(item.ItemClass) ||
						actorContainsItem(collectSettings.GetSeenActors(), manufacturer, item.ItemClass))
					{
						continue;
					}

					if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
					{
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Item amount = "), item.Amount);
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Current potential = "), manufacturer->GetCurrentPotential());
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Pending potential = "), manufacturer->GetPendingPotential());
						EC_LOG_Display(
							*collectSettings.GetIndent(),
							TEXT("    Production cycle time = "),
							manufacturer->CalcProductionCycleTimeForPotential(manufacturer->GetPendingPotential())
							);
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Recipe duration = "), UFGRecipe::GetManufacturingDuration(recipeClass));
					}

					float itemAmountPerMinute = item.Amount * manufacturer->GetPendingPotential() * 60
						/ manufacturer->GetDefaultProductionCycleTime();

					if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
					{
						itemAmountPerMinute /= 1000;
					}

					EC_LOG_Display_Condition(
						/**getTimeStamp(),*/
						*collectSettings.GetIndent(),
						TEXT("    "),
						*manufacturer->GetName(),
						TEXT(" consumes "),
						itemAmountPerMinute,
						TEXT(" "),
						*UFGItemDescriptor::GetItemName(item.ItemClass).ToString(),
						TEXT("/minute")
						);

					collectSettings.GetRequiredOutput()[item.ItemClass];

					if (!collectSettings.GetCustomRequiredOutput())
					{
						collectSettings.GetRequiredOutput()[item.ItemClass] += itemAmountPerMinute;
					}

					collectSettings.GetSeenActors()[manufacturer].Add(item.ItemClass);
				}
			}
		}
	}
}

void AEfficiencyCheckerLogic2::handleExtractor(AFGBuildableResourceExtractor* extractor, CollectSettings& collectSettings)
{
	if ((extractor->HasPower() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Unpowered)) &&
		(!extractor->IsProductionPaused() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Paused)))
	{
		TSubclassOf<UFGItemDescriptor> item;

		const auto resource = extractor->GetExtractableResource();

		auto speedMultiplier = resource ? resource->GetExtractionSpeedMultiplier() : 1;

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*collectSettings.GetIndent(),
			TEXT("    Extraction Speed Multiplier = "),
			speedMultiplier
			);

		if (!item)
		{
			if (resource)
			{
				item = resource->GetResourceClass();
			}
			else
			{
				EC_LOG_Display_Condition(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("Extractable resource is null")
					);
			}
		}

		if (!item)
		{
			item = extractor->GetOutputInventory()->GetAllowedItemOnIndex(0);
		}

		if (!item || !collectSettings.GetCurrentFilter().itemIsAllowed(item))
		{
			return;
		}

		EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("    Resource name = "), *UFGItemDescriptor::GetItemName(item).ToString());

		collectSettings.GetRequiredOutput()[item];
		//collectSettings.GetInjectedItems().Add(item);

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Current potential = "), extractor->GetCurrentPotential());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Pending potential = "), extractor->GetPendingPotential());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Default cycle time = "), extractor->GetDefaultExtractCycleTime());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Production cycle time = "), extractor->GetProductionCycleTime());
			EC_LOG_Display(
				*collectSettings.GetIndent(),
				TEXT("    Production cycle time for potential = "),
				extractor->CalcProductionCycleTimeForPotential(extractor->GetPendingPotential())
				);
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Items per cycle converted = "), extractor->GetNumExtractedItemsPerCycleConverted());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("    Items per cycle = "), extractor->GetNumExtractedItemsPerCycle());
		}

		float itemAmountPerMinute;

		const auto fullClassName = GetPathNameSafe(extractor->GetClass());

		if (fullClassName.EndsWith(TEXT("/Miner_Mk4/Build_MinerMk4.Build_MinerMk4_C")))
		{
			itemAmountPerMinute = 2000;
		}
		else
		{
			auto itemsPerCycle = extractor->GetNumExtractedItemsPerCycle();
			auto pendingPotential = extractor->GetPendingPotential();
			auto defaultExtractCycleTime = extractor->GetDefaultExtractCycleTime();

			itemAmountPerMinute = itemsPerCycle * pendingPotential * 60 /
				(speedMultiplier * defaultExtractCycleTime);

			if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
			{
				itemAmountPerMinute /= 1000;
			}
		}

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*collectSettings.GetIndent(),
			*extractor->GetName(),
			TEXT(" extracts "),
			itemAmountPerMinute,
			TEXT(" "),
			*UFGItemDescriptor::GetItemName(item).ToString(),
			TEXT("/minute")
			);

		if (!collectSettings.GetCustomInjectedInput())
		{
			collectSettings.GetInjectedInput()[item] += itemAmountPerMinute;
		}

		collectSettings.GetConnected().Add(extractor);
	}
}

void AEfficiencyCheckerLogic2::handleGeneratorFuel(AFGBuildableGeneratorFuel* generatorFuel, CollectSettings& collectSettings)
{
	if ((generatorFuel->HasPower() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Unpowered)) &&
		(!generatorFuel->IsProductionPaused() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Paused)))
	{
		if (collectSettings.GetInjectedInput().contains(generatorFuel->GetSupplementalResourceClass()) &&
			!collectSettings.GetSeenActors()[generatorFuel].Contains(generatorFuel->GetSupplementalResourceClass()))
		{
			if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
			{
				EC_LOG_Display(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("Supplemental item = "),
					*UFGItemDescriptor::GetItemName(generatorFuel->GetSupplementalResourceClass()).ToString()
					);
				EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Supplemental amount = "), generatorFuel->GetSupplementalConsumptionRateMaximum());
			}

			collectSettings.GetRequiredOutput()[generatorFuel->GetSupplementalResourceClass()] += generatorFuel->GetSupplementalConsumptionRateMaximum() * (
				(UFGItemDescriptor::GetForm(generatorFuel->GetSupplementalResourceClass()) == EResourceForm::RF_LIQUID ||
					UFGItemDescriptor::GetForm(generatorFuel->GetSupplementalResourceClass()) == EResourceForm::RF_GAS)
					? 60
					: 1);

			collectSettings.GetSeenActors()[generatorFuel].Add(generatorFuel->GetSupplementalResourceClass());
		}
		else
		{
			for (const auto& entry : collectSettings.GetInjectedInput())
			{
				auto item = entry.first;

				if (collectSettings.GetTimeout() < time(NULL))
				{
					EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating injected items!"));

					collectSettings.SetOverflow(true);
					return;
				}

				if (generatorFuel->IsValidFuel(item) && !collectSettings.GetSeenActors()[generatorFuel].Contains(item))
				{
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Energy item = "), *UFGItemDescriptor::GetItemName(item).ToString());

					float energy = UFGItemDescriptor::GetEnergyValue(item);

					// if (UFGItemDescriptor::GetForm(out_injectedItem) == EResourceForm::RF_LIQUID)
					// {
					//     energy *= 1000;
					// }

					if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
					{
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Energy = "), energy);
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Current potential = "), generatorFuel->GetCurrentPotential());
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Pending potential = "), generatorFuel->GetPendingPotential());
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Power production capacity = "), generatorFuel->GetPowerProductionCapacity());
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Default power production capacity = "), generatorFuel->GetDefaultPowerProductionCapacity());
					}

					float itemAmountPerMinute = 60 / (energy / generatorFuel->GetPowerProductionCapacity());

					if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
					{
						itemAmountPerMinute /= 1000;
					}

					EC_LOG_Display_Condition(
						/**getTimeStamp(),*/
						*collectSettings.GetIndent(),
						*generatorFuel->GetName(),
						TEXT(" consumes "),
						itemAmountPerMinute,
						TEXT(" "),
						*UFGItemDescriptor::GetItemName(item).ToString(),
						TEXT("/minute")
						);

					collectSettings.GetSeenActors()[generatorFuel].Add(item);

					collectSettings.GetRequiredOutput()[item] += itemAmountPerMinute;

					break;
				}
			}
		}

		collectSettings.GetConnected().Add(generatorFuel);
	}
}

void AEfficiencyCheckerLogic2::getFactoryConnectionComponents
(
	class AFGBuildable* buildable,
	std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	const std::function<bool (class UFGFactoryConnectionComponent*)>& filter
)
{
	TArray<UFGFactoryConnectionComponent*> tempComponents;
	buildable->GetComponents(tempComponents);

	for (auto component : tempComponents.FilterByPredicate(
		     [&filter](UFGFactoryConnectionComponent* connection)
		     {
			     return connection->IsConnected() && filter(connection);
		     }
		     )
		)
	{
		if (!inputComponents.contains(component) && component->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
		{
			inputComponents[component];
		}
		else if (!outputComponents.contains(component) && component->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
		{
			outputComponents[component];
		}
	}
}

void AEfficiencyCheckerLogic2::getPipeConnectionComponents
(
	class AFGBuildable* buildable,
	TSet<class UFGPipeConnectionComponent*>& anyDirectionComponents,
	TSet<class UFGPipeConnectionComponent*>& inputComponents,
	TSet<class UFGPipeConnectionComponent*>& outputComponents,
	const std::function<bool (class UFGPipeConnectionComponent*)>& filter
)
{
	TArray<UFGPipeConnectionComponent*> tempComponents;
	buildable->GetComponents(tempComponents);

	for (auto component : tempComponents.FilterByPredicate(
		     [&filter](UFGPipeConnectionComponent* connection)
		     {
			     return connection->IsConnected() && filter(connection);
		     }
		     )
		)
	{
		auto connectionType = component->GetPipeConnectionType();
		auto otherConnectionType = getConnectedPipeConnectionType(component);

		if (connectionType == EPipeConnectionType::PCT_ANY && otherConnectionType == EPipeConnectionType::PCT_ANY)
		{
			anyDirectionComponents.Add(component);
		}
		else if ((connectionType == EPipeConnectionType::PCT_ANY || connectionType == EPipeConnectionType::PCT_CONSUMER) &&
			(otherConnectionType == EPipeConnectionType::PCT_ANY || otherConnectionType == EPipeConnectionType::PCT_PRODUCER))
		{
			inputComponents.Add(component);
		}
		else if ((connectionType == EPipeConnectionType::PCT_ANY || connectionType == EPipeConnectionType::PCT_PRODUCER) &&
			(otherConnectionType == EPipeConnectionType::PCT_ANY || otherConnectionType == EPipeConnectionType::PCT_CONSUMER))
		{
			outputComponents.Add(component);
		}
	}
}

void AEfficiencyCheckerLogic2::handleUndergroundBeltsComponents
(
	AFGBuildableStorage* undergroundBelt,
	CollectSettings& collectSettings,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
)
{
	auto commonInfoSubsystem = ACommonInfoSubsystem::Get();
	
	getFactoryConnectionComponents(undergroundBelt, inputComponents, outputComponents);

	FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);
	
	if (commonInfoSubsystem->baseUndergroundSplitterInputClass &&
		undergroundBelt->IsA(commonInfoSubsystem->baseUndergroundSplitterInputClass))
	{
		auto outputsProperty = CastField<FArrayProperty>(undergroundBelt->GetClass()->FindPropertyByName("Outputs"));
		if (!outputsProperty)
		{
			return;
		}

		FScriptArrayHelper arrayHelper(outputsProperty, outputsProperty->ContainerPtrToValuePtr<void>(undergroundBelt));
		auto arrayObjectProperty = CastField<FObjectProperty>(outputsProperty->Inner);

		for (auto x = 0; x < arrayHelper.Num(); x++)
		{
			void* ObjectContainer = arrayHelper.GetRawPtr(x);
			auto outputUndergroundBelt = Cast<AFGBuildableFactory>(arrayObjectProperty->GetObjectPropertyValue(ObjectContainer));

			if (outputUndergroundBelt)
			{
				collectSettings.GetSeenActors()[outputUndergroundBelt];
				collectSettings.GetConnected().Add(outputUndergroundBelt);

				getFactoryConnectionComponents(
					outputUndergroundBelt,
					inputComponents,
					outputComponents,
					[outputComponents](UFGFactoryConnectionComponent* connection)
					{
						return !outputComponents.contains(connection) && connection->IsConnected() && connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT;
						// Is output connection
					}
					);
			}
		}
	}
	else if (commonInfoSubsystem->baseUndergroundSplitterOutputClass &&
		undergroundBelt->IsA(commonInfoSubsystem->baseUndergroundSplitterOutputClass))
	{
		for (auto inputUndergroundBelt : commonInfoSubsystem->allUndergroundInputBelts)
		{
			auto outputsProperty = CastField<FArrayProperty>(inputUndergroundBelt->GetClass()->FindPropertyByName("Outputs"));
			if (!outputsProperty)
			{
				continue;
			}

			FScriptArrayHelper arrayHelper(outputsProperty, outputsProperty->ContainerPtrToValuePtr<void>(inputUndergroundBelt));
			auto arrayObjectProperty = CastField<FObjectProperty>(outputsProperty->Inner);

			auto found = false;

			for (auto x = 0; !found && x < arrayHelper.Num(); x++)
			{
				void* ObjectContainer = arrayHelper.GetRawPtr(x);
				found = undergroundBelt == arrayObjectProperty->GetObjectPropertyValue(ObjectContainer);
			}

			if (!found)
			{
				continue;
			}

			addAllItemsToActor(collectSettings, inputUndergroundBelt);
			collectSettings.GetConnected().Add(inputUndergroundBelt);

			getFactoryConnectionComponents(
				inputUndergroundBelt,
				inputComponents,
				outputComponents,
				[outputComponents](UFGFactoryConnectionComponent* connection)
				{
					return !outputComponents.contains(connection) && connection->IsConnected() && connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT;
					// Is input connection
				}
				);
		}
	}
}

void AEfficiencyCheckerLogic2::handleContainerComponents
(
	AFGBuildable* buildable,
	UFGInventoryComponent* inventory,
	CollectSettings& collectSettings,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	bool collectForInput,
	const std::function<bool (class UFGFactoryConnectionComponent*)>& filter
)
{
	getFactoryConnectionComponents(buildable, inputComponents, outputComponents, filter);

	TArray<FInventoryStack> stacks;

	inventory->GetInventoryStacks(stacks);

	for (const auto& stack : stacks)
	{
		if (!collectSettings.GetCurrentFilter().itemIsAllowed(stack.Item.GetItemClass()))
		{
			continue;
		}

		if (collectForInput)
		{
			collectSettings.GetInjectedInput()[stack.Item.GetItemClass()];
		}
		else
		{
			collectSettings.GetRequiredOutput()[stack.Item.GetItemClass()];
		}
	}
}

void AEfficiencyCheckerLogic2::handleTrainPlatformCargoBelt
(
	AFGBuildableTrainPlatformCargo* trainPlatformCargo,
	CollectSettings& collectSettings,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	bool collectForInput
)
{
	handleContainerComponents(trainPlatformCargo, trainPlatformCargo->GetInventory(), collectSettings, inputComponents, outputComponents, collectForInput);

	if (collectForInput)
	{
		collectSettings.GetSeenActors()[trainPlatformCargo];
	}
	else
	{
		addAllItemsToActor(collectSettings, trainPlatformCargo);
	}

	auto trackId = trainPlatformCargo->GetTrackGraphID();

	auto railroadSubsystem = AFGRailroadSubsystem::Get(trainPlatformCargo->GetWorld());

	// Determine offsets from all the connected stations
	std::set<int> stationOffsets;
	TSet<AFGBuildableRailroadStation*> destinationStations;

	for (auto i = 0; i <= 1; i++)
	{
		auto offsetDistance = 1;

		TSet<AFGBuildableTrainPlatform*> seenPlatforms;

		for (auto connectedPlatform = trainPlatformCargo->GetConnectedPlatformInDirectionOf(i);
		     connectedPlatform;
		     connectedPlatform = connectedPlatform->GetConnectedPlatformInDirectionOf(i),
		     ++offsetDistance)
		{
			if (collectSettings.GetTimeout() < time(NULL))
			{
				EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

				collectSettings.SetOverflow(true);
				return;
			}

			if (seenPlatforms.Contains(connectedPlatform))
			{
				// Loop detected
				break;
			}

			seenPlatforms.Add(connectedPlatform);

			EC_LOG_Display_Condition(
				/**getTimeStamp(),*/
				*collectSettings.GetIndent(),
				*connectedPlatform->GetName(),
				TEXT(" direction = "),
				i,
				TEXT(" / orientation reversed = "),
				connectedPlatform->IsOrientationReversed() ? TEXT("true") : TEXT("false")
				);

			auto station = Cast<AFGBuildableRailroadStation>(connectedPlatform);
			if (station)
			{
				destinationStations.Add(station);

				EC_LOG_Display_Condition(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("    Station = "),
					*station->GetStationIdentifier()->GetStationName().ToString()
					);

				if (i == 0 && connectedPlatform->IsOrientationReversed() ||
					i == 1 && !connectedPlatform->IsOrientationReversed())
				{
					stationOffsets.insert(offsetDistance);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("        offset distance = "), offsetDistance);
				}
				else
				{
					stationOffsets.insert(-offsetDistance);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("        offset distance = "), -offsetDistance);
				}
			}

			if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
			{
				auto cargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
				if (cargo)
				{
					EC_LOG_Display(
						/**getTimeStamp(),*/
						*collectSettings.GetIndent(),
						TEXT("    Load mode = "),
						cargo->GetIsInLoadMode() ? TEXT("true") : TEXT("false")
						);
				}
			}
		}
	}

	TArray<AFGTrain*> trains;
	railroadSubsystem->GetTrains(trackId, trains);

	for (auto train : trains)
	{
		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains!"));

			collectSettings.SetOverflow(true);
			return;
		}

		if (!train->HasTimeTable())
		{
			continue;
		}

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			if (!train->GetTrainName().IsEmpty())
			{
				EC_LOG_Display(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("Train = "),
					*train->GetTrainName().ToString()
					);
			}
			else
			{
				EC_LOG_Display(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("Anonymous Train")
					);
			}
		}

		// Get train stations
		auto timeTable = train->GetTimeTable();

		TArray<FTimeTableStop> stops;
		timeTable->GetStops(stops);

		bool stopAtStations = false;

		for (const auto& stop : stops)
		{
			if (collectSettings.GetTimeout() < time(NULL))
			{
				EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout iterating train stops!"));

				collectSettings.SetOverflow(true);
				return;
			}

			if (!stop.Station || !stop.Station->GetStation() || !destinationStations.Contains(stop.Station->GetStation()))
			{
				continue;
			}

			stopAtStations = true;

			break;
		}

		if (!stopAtStations)
		{
			continue;
		}

		for (const auto& stop : stops)
		{
			if (collectSettings.GetTimeout() < time(NULL))
			{
				EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout iterating train stops!"));

				collectSettings.SetOverflow(true);
				return;
			}

			if (!stop.Station || !stop.Station->GetStation())
			{
				continue;
			}

			EC_LOG_Display_Condition(
				/**getTimeStamp(),*/
				*collectSettings.GetIndent(),
				TEXT("    Stop = "),
				*stop.Station->GetStationName().ToString()
				);

			for (auto i = 0; i <= 1; i++)
			{
				auto offsetDistance = 1;

				TSet<AFGBuildableTrainPlatform*> seenPlatforms;

				for (auto connectedPlatform = stop.Station->GetStation()->GetConnectedPlatformInDirectionOf(i);
				     connectedPlatform;
				     connectedPlatform = connectedPlatform->GetConnectedPlatformInDirectionOf(i),
				     ++offsetDistance)
				{
					if (collectSettings.GetTimeout() < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platformst!"));

						collectSettings.SetOverflow(true);
						return;
					}

					if (seenPlatforms.Contains(connectedPlatform))
					{
						// Loop detected
						break;
					}

					auto stopCargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
					if (!stopCargo || stopCargo == trainPlatformCargo)
					{
						// Not a cargo or the same as the current one. Skip
						continue;
					}

					seenPlatforms.Add(stopCargo);

					auto adjustedOffsetDistance = i == 0 && !stop.Station->GetStation()->IsOrientationReversed()
					                              || i == 1 && stop.Station->GetStation()->IsOrientationReversed()
						                              ? offsetDistance
						                              : -offsetDistance;

					if (stationOffsets.find(adjustedOffsetDistance) == stationOffsets.end())
					{
						// Not on a valid offset. Skip
						continue;
					}

					collectSettings.GetConnected().Add(stopCargo);

					if (collectForInput)
					{
						collectSettings.GetSeenActors()[stopCargo];
					}
					else
					{
						addAllItemsToActor(collectSettings, stopCargo);
					}

					getFactoryConnectionComponents(
						stopCargo,
						inputComponents,
						outputComponents,
						[stopCargo, collectForInput](UFGFactoryConnectionComponent* connection)
						{
							if (collectForInput)
							{
								// It is for input collection

								// When in loading mode, include all connections. Also include if it is a producer, regardless of loading mode
								return stopCargo->GetIsInLoadMode() || connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT;
							}
							else
							{
								// It is for output collection

								// When in unloading mode, include all connections. Also include if it is a consumer, regardless of loading mode
								return !stopCargo->GetIsInLoadMode() || connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT;
							}
						}
						);
				}
			}
		}
	}
}

void AEfficiencyCheckerLogic2::handleTrainPlatformCargoPipe
(
	class AFGBuildableTrainPlatformCargo* trainPlatformCargo,
	class CollectSettings& collectSettings,
	TSet<class UFGPipeConnectionComponent*>& inputComponents,
	TSet<class UFGPipeConnectionComponent*>& outputComponents,
	bool collectForInput
)
{
	if (collectForInput)
	{
		collectSettings.GetSeenActors()[trainPlatformCargo];
	}
	else
	{
		addAllItemsToActor(collectSettings, trainPlatformCargo);
	}

	// Add all local pipes
	TSet<class UFGPipeConnectionComponent*> anyDirection;

	getPipeConnectionComponents(
		trainPlatformCargo,
		anyDirection,
		inputComponents,
		outputComponents
		);

	auto trackId = trainPlatformCargo->GetTrackGraphID();

	auto railroadSubsystem = AFGRailroadSubsystem::Get(trainPlatformCargo->GetWorld());

	// Determine offsets from all the connected stations
	std::set<int> stationOffsets;
	TSet<AFGBuildableRailroadStation*> destinationStations;

	for (auto i = 0; i <= 1; i++)
	{
		auto offsetDistance = 1;

		TSet<AFGBuildableTrainPlatform*> seenPlatforms;

		for (auto connectedPlatform = trainPlatformCargo->GetConnectedPlatformInDirectionOf(i);
		     connectedPlatform;
		     connectedPlatform = connectedPlatform->GetConnectedPlatformInDirectionOf(i),
		     ++offsetDistance)
		{
			if (collectSettings.GetTimeout() < time(NULL))
			{
				EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

				collectSettings.SetOverflow(true);
				return;
			}

			if (seenPlatforms.Contains(connectedPlatform))
			{
				// Loop detected
				break;
			}

			EC_LOG_Display_Condition(
				/**getTimeStamp(),*/
				*collectSettings.GetIndent(),
				*connectedPlatform->GetName(),
				TEXT(" direction = "),
				i,
				TEXT(" / orientation reversed = "),
				connectedPlatform->IsOrientationReversed() ? TEXT("true") : TEXT("false")
				);

			auto station = Cast<AFGBuildableRailroadStation>(connectedPlatform);
			if (station)
			{
				destinationStations.Add(station);

				EC_LOG_Display_Condition(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("    Station = "),
					*station->GetStationIdentifier()->GetStationName().ToString()
					);

				if (i == 0 && connectedPlatform->IsOrientationReversed() ||
					i == 1 && !connectedPlatform->IsOrientationReversed())
				{
					stationOffsets.insert(offsetDistance);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("        offset distance = "), offsetDistance);
				}
				else
				{
					stationOffsets.insert(-offsetDistance);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("        offset distance = "), -offsetDistance);
				}
			}

			if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
			{
				auto cargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
				if (cargo)
				{
					EC_LOG_Display(
						/**getTimeStamp(),*/
						*collectSettings.GetIndent(),
						TEXT("    Load mode = "),
						cargo->GetIsInLoadMode() ? TEXT("true") : TEXT("false")
						);
				}
			}
		}
	}

	TArray<AFGTrain*> trains;
	railroadSubsystem->GetTrains(trackId, trains);

	for (auto train : trains)
	{
		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains!"));

			collectSettings.SetOverflow(true);
			return;
		}

		if (!train->HasTimeTable())
		{
			continue;
		}

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			if (!train->GetTrainName().IsEmpty())
			{
				EC_LOG_Display(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("Train = "),
					*train->GetTrainName().ToString()
					);
			}
			else
			{
				EC_LOG_Display(
					/**getTimeStamp(),*/
					*collectSettings.GetIndent(),
					TEXT("Anonymous Train")
					);
			}
		}

		// Get train stations
		auto timeTable = train->GetTimeTable();

		TArray<FTimeTableStop> stops;
		timeTable->GetStops(stops);

		bool stopAtStations = false;

		for (const auto& stop : stops)
		{
			if (collectSettings.GetTimeout() < time(NULL))
			{
				EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

				collectSettings.SetOverflow(true);
				return;
			}

			if (!stop.Station || !stop.Station->GetStation() || !destinationStations.Contains(stop.Station->GetStation()))
			{
				continue;
			}

			stopAtStations = true;

			break;
		}

		if (!stopAtStations)
		{
			continue;
		}

		for (const auto& stop : stops)
		{
			if (collectSettings.GetTimeout() < time(NULL))
			{
				EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

				collectSettings.SetOverflow(true);
				return;
			}

			if (!stop.Station || !stop.Station->GetStation())
			{
				continue;
			}

			EC_LOG_Display_Condition(
				/**getTimeStamp(),*/
				*collectSettings.GetIndent(),
				TEXT("    Stop = "),
				*stop.Station->GetStationName().ToString()
				);

			for (auto i = 0; i <= 1; i++)
			{
				auto offsetDistance = 1;

				TSet<AFGBuildableTrainPlatform*> seenPlatforms;

				for (auto connectedPlatform = stop.Station->GetStation()->GetConnectedPlatformInDirectionOf(i);
				     connectedPlatform;
				     connectedPlatform = connectedPlatform->GetConnectedPlatformInDirectionOf(i),
				     ++offsetDistance)
				{
					if (collectSettings.GetTimeout() < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

						collectSettings.SetOverflow(true);
						return;
					}

					if (seenPlatforms.Contains(connectedPlatform))
					{
						// Loop detected
						break;
					}

					auto stopCargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
					if (!stopCargo || stopCargo == trainPlatformCargo)
					{
						// Not a cargo or the same as the current one. Skip
						continue;
					}

					auto adjustedOffsetDistance = i == 0 && !stop.Station->GetStation()->IsOrientationReversed()
					                              || i == 1 && stop.Station->GetStation()->IsOrientationReversed()
						                              ? offsetDistance
						                              : -offsetDistance;

					if (stationOffsets.find(adjustedOffsetDistance) == stationOffsets.end())
					{
						// Not on a valid offset. Skip
						continue;
					}

					collectSettings.GetConnected().Add(stopCargo);

					if (collectForInput)
					{
						collectSettings.GetSeenActors()[stopCargo];
					}
					else
					{
						addAllItemsToActor(collectSettings, stopCargo);
					}

					getPipeConnectionComponents(
						stopCargo,
						anyDirection,
						inputComponents,
						outputComponents,
						[stopCargo, collectForInput](UFGPipeConnectionComponent* connection)
						{
							if (collectForInput)
							{
								// It is for input collection

								// When in loading mode, include all connections. Also include if it is a producer, regardless of loading mode
								return stopCargo->GetIsInLoadMode() || connection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER;
							}
							else
							{
								// It is for output collection

								// When in unloading mode, include all connections. Also include if it is a consumer, regardless of loading mode
								return !stopCargo->GetIsInLoadMode() || connection->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER;
							}
						}
						);
				}
			}
		}
	}
}

void AEfficiencyCheckerLogic2::handleStorageTeleporter
(
	AFGBuildable* storageTeleporter,
	CollectSettings& collectSettings,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	bool collectForInput
)
{
	// Find all others of the same type
	auto currentStorageID = FReflectionHelper::GetPropertyValue<FStrProperty>(storageTeleporter, TEXT("StorageID"));

	getFactoryConnectionComponents(storageTeleporter, inputComponents, outputComponents);

	auto commonInfoSubsystem = ACommonInfoSubsystem::Get();

	FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);

	for (auto testTeleporter : commonInfoSubsystem->allTeleporters)
	{
		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating storage teleporters!"));

			collectSettings.SetOverflow(true);
			return;
		}

		if (!IsValid(testTeleporter) || testTeleporter == storageTeleporter)
		{
			continue;
		}

		auto storageID = FReflectionHelper::GetPropertyValue<FStrProperty>(testTeleporter, TEXT("StorageID"));
		if (storageID != currentStorageID)
		{
			continue;
		}

		if (collectForInput)
		{
			collectSettings.GetSeenActors()[testTeleporter];
		}
		else
		{
			addAllItemsToActor(collectSettings, testTeleporter);
		}
		collectSettings.GetConnected().Add(testTeleporter);

		getFactoryConnectionComponents(testTeleporter, inputComponents, outputComponents);
	}
}

void AEfficiencyCheckerLogic2::handleModularLoadBalancerComponents
(
	AFGBuildableFactory* modularLoadBalancerGroupLeader,
	CollectSettings& collectSettings,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	bool collectForInput
)
{
	auto groupModulesArrayProperty = CastField<FArrayProperty>(modularLoadBalancerGroupLeader->GetClass()->FindPropertyByName(TEXT("mGroupModules")));
	if (!groupModulesArrayProperty)
	{
		return;
	}

	if (groupModulesArrayProperty)
	{
		FScriptArrayHelper arrayHelper(groupModulesArrayProperty, groupModulesArrayProperty->ContainerPtrToValuePtr<void>(modularLoadBalancerGroupLeader));

		auto arrayWeakObjectProperty = CastField<FWeakObjectProperty>(groupModulesArrayProperty->Inner);

		// TSet<TSubclassOf<UFGItemDescriptor>> definedItems;
		// TSet<UFGFactoryConnectionComponent*> overflowComponents;

		for (auto x = 0; x < arrayHelper.Num(); x++)
		{
			void* modularLoadBalancerObjectContainer = arrayHelper.GetRawPtr(x);
			auto modularLoadBalancer = Cast<AFGBuildableFactory>(arrayWeakObjectProperty->GetObjectPropertyValue(modularLoadBalancerObjectContainer));
			if (!modularLoadBalancer)
			{
				continue;
			}

			if (collectForInput)
			{
				collectSettings.GetSeenActors()[modularLoadBalancer];
			}
			else
			{
				addAllItemsToActor(collectSettings, modularLoadBalancer);
			}

			std::map<UFGFactoryConnectionComponent*, FComponentFilter> tempOutputComponents;

			getFactoryConnectionComponents(modularLoadBalancer, inputComponents, tempOutputComponents);

			for (const auto& entry : tempOutputComponents)
			{
				outputComponents[entry.first] = entry.second;
			}

			auto filteredItemsProperty = CastField<FArrayProperty>(modularLoadBalancer->GetClass()->FindPropertyByName(TEXT("mFilteredItems")));
			auto loaderTypeProperty = CastField<FEnumProperty>(modularLoadBalancer->GetClass()->FindPropertyByName(TEXT("mLoaderType")));
			if (filteredItemsProperty && loaderTypeProperty)
			{
				const auto objReflection = UBlueprintReflectionLibrary::ReflectObject(modularLoadBalancer);
				const auto reflectedValue = objReflection.GetEnumProperty(FName(loaderTypeProperty->GetName()));
				auto currentValue = reflectedValue.GetCurrentValue();

				FScriptArrayHelper filteredItemsHelper(filteredItemsProperty, filteredItemsProperty->ContainerPtrToValuePtr<void>(modularLoadBalancer));

				auto filteredItemArrayObjectProperty = CastField<FObjectProperty>(filteredItemsProperty->Inner);

				TSet<TSubclassOf<UFGItemDescriptor>> filteredItems;

				for (auto x2 = 0; x2 < filteredItemsHelper.Num(); x2++)
				{
					void* itemObjectContainer = filteredItemsHelper.GetRawPtr(x2);
					auto item = Cast<UClass>(filteredItemArrayObjectProperty->GetObjectPropertyValue(itemObjectContainer));
					if (!item)
					{
						continue;
					}

					// definedItems.Add(item);
					filteredItems.Add(item);
				}

				switch (currentValue)
				{
				// case 1: // Overflow
				// 	for (const auto& component : tempOutputComponents)
				// 	{
				// 		overflowComponents.Add(component.first);
				// 	}
				// 	break;

				case 2: // Filter
				case 3: // Programmable
					for (const auto& component : tempOutputComponents)
					{
						outputComponents[component.first].allowedFiltered = true;
						outputComponents[component.first].allowedItems = filteredItems;
					}
					break;

				default:
					break;
				}
			}
		}

		// for (auto component : overflowComponents)
		// {
		// 	outputComponents[component].deniedFiltered = true;
		// 	outputComponents[component].deniedItems = definedItems;
		// }
	}
}

void AEfficiencyCheckerLogic2::handleSmartSplitterComponents
(
	AFGBuildableSplitterSmart* smartSplitter,
	CollectSettings& collectSettings,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	std::map<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	bool collectForInput
)
{
	auto commonInfoSubsystem = ACommonInfoSubsystem::Get();
	
	std::map<UFGFactoryConnectionComponent*, FComponentFilter> tempInputComponents;
	std::map<UFGFactoryConnectionComponent*, FComponentFilter> tempOutputComponents;

	getFactoryConnectionComponents(smartSplitter, tempInputComponents, tempOutputComponents);

	std::map<int, UFGFactoryConnectionComponent*> outputComponentsMapByIndex;

	for (const auto& entry : tempOutputComponents)
	{
		FRegexMatcher m(AEfficiencyCheckerLogic::indexPattern, entry.first->GetName());
		if (m.FindNext())
		{
			auto index = FCString::Atoi(*m.GetCaptureGroup(1)) - 1;

			outputComponentsMapByIndex[index] = entry.first;
		}
	}

	// Already restricted. Restrict further
	for (int x = 0; x < smartSplitter->GetNumSortRules(); ++x)
	{
		auto rule = smartSplitter->GetSortRuleAt(x);

		if (!outputComponentsMapByIndex.contains(rule.OutputIndex))
		{
			// The connector is not connect or is not valid
			continue;
		}

		auto connection = outputComponentsMapByIndex[rule.OutputIndex];

		auto& componentFilter = tempOutputComponents[connection];

		componentFilter.allowedFiltered = true;
		componentFilter.allowedItems.Add(rule.ItemClass);

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*collectSettings.GetIndent(),
			TEXT("Rule "),
			x,
			TEXT(" / output index = "),
			rule.OutputIndex,
			TEXT(" / item = "),
			*UFGItemDescriptor::GetItemName(rule.ItemClass).ToString(),
			TEXT(" / class = "),
			*GetPathNameSafe(rule.ItemClass),
			TEXT(" / connection = "),
			*GetNameSafe(connection)
			);
	}

	TSet<TSubclassOf<UFGItemDescriptor>> definedItems;

	// First pass
	for (auto& entry : tempOutputComponents)
	{
		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating restricted items!"));

			collectSettings.SetOverflow(true);
			return;
		}

		if (!commonInfoSubsystem->noneItemDescriptors.Intersect(entry.second.allowedItems).IsEmpty())
		{
			// No item is valid. Empty it all
			entry.second.allowedItems.Empty();
		}
		else if (!commonInfoSubsystem->wildCardItemDescriptors.Intersect(entry.second.allowedItems).IsEmpty() ||
			!commonInfoSubsystem->overflowItemDescriptors.Intersect(entry.second.allowedItems).IsEmpty())
		{
			// Remove restrictions. Any item can flow through
			entry.second.allowedFiltered = false;
			entry.second.allowedItems.Empty();
		}

		definedItems.Append(entry.second.allowedItems);
	}

	// Second pass
	for (auto& entry : tempOutputComponents)
	{
		if (collectSettings.GetTimeout() < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating restricted items!"));

			collectSettings.SetOverflow(true);
			return;
		}

		if (!commonInfoSubsystem->anyUndefinedItemDescriptors.Intersect(entry.second.allowedItems).IsEmpty())
		{
			entry.second.allowedFiltered = false;
			entry.second.allowedItems.Empty();
			entry.second.deniedFiltered = true;
			entry.second.deniedItems.Append(definedItems);
		}

		entry.second.allowedItems = entry.second.allowedItems
		                                 .Difference(commonInfoSubsystem->noneItemDescriptors)
		                                 .Difference(commonInfoSubsystem->wildCardItemDescriptors)
		                                 .Difference(commonInfoSubsystem->anyUndefinedItemDescriptors)
		                                 .Difference(commonInfoSubsystem->overflowItemDescriptors);

		entry.second.deniedItems = entry.second.deniedItems
		                                .Difference(commonInfoSubsystem->noneItemDescriptors)
		                                .Difference(commonInfoSubsystem->wildCardItemDescriptors)
		                                .Difference(commonInfoSubsystem->anyUndefinedItemDescriptors)
		                                .Difference(commonInfoSubsystem->overflowItemDescriptors);

		auto temp = FComponentFilter::combineFilters(entry.second, collectSettings.GetCurrentFilter());

		entry.second.allowedFiltered = temp.allowedFiltered;
		entry.second.allowedItems = temp.allowedItems;
		entry.second.deniedFiltered = temp.deniedFiltered;
		entry.second.deniedItems = temp.deniedItems;

		if (entry.first == collectSettings.GetConnector() && entry.second.allowedFiltered && entry.second.allowedItems.IsEmpty())
		{
			// Can't go further. Return
			return;
		}
	}

	if (collectForInput &&
		std::find_if(
			tempOutputComponents.begin(),
			tempOutputComponents.end(),
			[](const auto& entry)
			{
				return !entry.second.allowedFiltered || !entry.second.allowedItems.IsEmpty();
			}
			) != tempOutputComponents.end())
	{
		// Nothing will flow through. Return
		return;
	}

	definedItems = definedItems.Difference(commonInfoSubsystem->noneItemDescriptors);

	auto allFiltered = std::find_if(
		tempOutputComponents.begin(),
		tempOutputComponents.end(),
		[](const auto& entry)
		{
			return !entry.second.allowedFiltered;
		}
		) == tempOutputComponents.end();

	for (auto& it : tempInputComponents)
	{
		if (!definedItems.Intersect(commonInfoSubsystem->anyUndefinedItemDescriptors).IsEmpty() ||
			!definedItems.Intersect(commonInfoSubsystem->wildCardItemDescriptors).IsEmpty() ||
			!definedItems.Intersect(commonInfoSubsystem->overflowItemDescriptors).IsEmpty())
		{
			// Allow anything to pass-through
			it.second.allowedFiltered = collectSettings.GetCurrentFilter().allowedFiltered;
			it.second.allowedItems = collectSettings.GetCurrentFilter().allowedItems;
			it.second.deniedFiltered = collectSettings.GetCurrentFilter().deniedFiltered;
			it.second.deniedItems = collectSettings.GetCurrentFilter().deniedItems;
		}
		else
		{
			it.second.allowedFiltered = allFiltered;

			if (it.second.allowedFiltered)
			{
				if (collectSettings.GetCurrentFilter().allowedFiltered)
				{
					it.second.allowedItems = collectSettings.GetCurrentFilter().allowedItems.Intersect(definedItems);
				}

				if (it.second.allowedItems.IsEmpty())
				{
					continue;
				}
			}
		}

		inputComponents[it.first] = it.second;
	}

	for (const auto& entry : tempOutputComponents)
	{
		if (entry.second.allowedFiltered && entry.second.allowedItems.IsEmpty())
		{
			continue;
		}

		outputComponents[entry.first] = entry.second;
	}
}

void AEfficiencyCheckerLogic2::handleFluidIntegrant
(
	class IFGFluidIntegrantInterface* fluidIntegrant,
	class CollectSettings& collectSettings,
	TSet<class UFGPipeConnectionComponent*>& anyDirectionComponents,
	TSet<class UFGPipeConnectionComponent*>& inputComponents,
	TSet<class UFGPipeConnectionComponent*>& outputComponents
)
{
	auto pipeConnection = Cast<UFGPipeConnectionComponent>(collectSettings.GetConnector());

	getPipeConnectionComponents(
		Cast<AFGBuildable>(fluidIntegrant),
		anyDirectionComponents,
		inputComponents,
		outputComponents,
		[&collectSettings](UFGPipeConnectionComponent* connection)
		{
			return collectSettings.GetSeenActors().size() == 1 || connection != collectSettings.GetConnector();
		}
		);

	if (auto pipeline = Cast<AFGBuildablePipeline>(fluidIntegrant))
	{
		collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), getPipeSpeed(pipeline)));
	}

	auto pipePump = Cast<AFGBuildablePipelinePump>(fluidIntegrant);

	if (pipePump && anyDirectionComponents.IsEmpty() && inputComponents.Num() == 1 && outputComponents.Num() == 1)
	{
		if (pipePump->GetUserFlowLimit() == 0)
		{
			// Flow is blocked
			return;
		}
		else if (pipePump->GetUserFlowLimit() > 0)
		{
			auto firstOutput = getFirstItem(outputComponents);

			auto pipe1 = Cast<AFGBuildablePipeline>(getFirstItem(inputComponents)->GetPipeConnection()->GetOwner());
			auto pipe2 = Cast<AFGBuildablePipeline>(getFirstItem(outputComponents)->GetPipeConnection()->GetOwner());

			collectSettings.SetLimitedThroughput(
				FMath::Min(
					collectSettings.GetLimitedThroughput(),
					UFGBlueprintFunctionLibrary::RoundFloatWithPrecision(
						FMath::Min(
							getPipeSpeed(pipe1),
							getPipeSpeed(pipe2)
							) * pipePump->GetUserFlowLimit() / pipePump->GetDefaultFlowLimit(),
						4
						)
					)
				);
		}
		else
		{
			// No user flow limitation. Use max flow limit for the pump/valve
			collectSettings.SetLimitedThroughput(
				FMath::Min(
					collectSettings.GetLimitedThroughput(),
					pipePump->GetDefaultFlowLimit() * 60
					)
				);
		}
	}
}

void AEfficiencyCheckerLogic2::addAllItemsToActor
(
	CollectSettings& collectSettings,
	AActor* actor
)
{
	// Ensure the actor exists, even with an empty list
	TSet<TSubclassOf<UFGItemDescriptor>>& itemsSet = collectSettings.GetSeenActors()[actor];

	for (auto item : collectSettings.GetInjectedInput())
	{
		itemsSet.Add(item.first);
	}
}


#ifndef OPTIMIZE
#pragma optimize("", on)
#endif
