#include "Logic/EfficiencyCheckerLogic2.h"
#include "Util/EfficiencyCheckerOptimize.h"
#include "EfficiencyCheckerBuilding.h"
#include "EfficiencyChecker_ConfigStruct.h"
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
#include "Util/Logging.h"
#include "Util/Helpers.h"
#include "Util/EfficiencyCheckerConfiguration.h"

#include <set>

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

void AEfficiencyCheckerLogic2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
}

void AEfficiencyCheckerLogic2::collectInput(ICollectSettings& collectSettings)
{
	for (;;)
	{
		if (!collectSettings.GetConnector())
		{
			return;
		}

		auto owner = collectSettings.GetConnector()->GetOwner();

		if (!owner || collectSettings.GetSeenActors().Contains(owner))
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
			*fullClassName
			);

		collectSettings.GetSeenActors().Add(owner);

		{
			const auto manufacturer = Cast<AFGBuildableManufacturer>(owner);

			if (manufacturer)
			{
				handleManufacturer(
					manufacturer,
					collectSettings,
					true,
					false
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
			const auto conveyor = Cast<AFGBuildableConveyorBase>(owner);
			if (conveyor)
			{
				// The initial limit for a belt is its own speed
				collectSettings.GetConnected().Add(conveyor);

				collectSettings.SetConnector(conveyor->GetConnection0()->GetConnection());

				collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), conveyor->GetSpeed() / 2));

				EC_LOG_Display_Condition(*collectSettings.GetIndent(), *conveyor->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" items/minute"));

				continue;
			}

			AFGBuildable* buildable = nullptr;

			TMap<UFGFactoryConnectionComponent*, FComponentFilter> inputComponents, outputComponents;

			if (!buildable &&
				(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterInputClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterInputClass) ||
					AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterOutputClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterOutputClass)))
			{
				auto undergroundBelt = Cast<AFGBuildableStorage>(owner);
				buildable = undergroundBelt;

				if (undergroundBelt)
				{
					handleUndergroundBeltsComponents(undergroundBelt, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!AEfficiencyCheckerConfiguration::configuration.ignoreStorageTeleporter &&
				!buildable && AEfficiencyCheckerLogic::singleton->baseStorageTeleporterClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseStorageTeleporterClass))
			{
				auto storageTeleporter = Cast<AFGBuildableFactory>(owner);
				buildable = storageTeleporter;

				if (storageTeleporter)
				{
					handleStorageTeleporter(storageTeleporter, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!buildable && AEfficiencyCheckerLogic::singleton->baseModularLoadBalancerClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseModularLoadBalancerClass))
			{
				auto modularLoadBalancer = FReflectionHelper::GetObjectPropertyValue<AFGBuildableFactory>(owner, TEXT("GroupLeader"));
				buildable = modularLoadBalancer;

				if (modularLoadBalancer)
				{
					handleModularLoadBalancerComponents(modularLoadBalancer, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				buildable = Cast<AFGBuildableConveyorAttachment>(owner);
				if (buildable)
				{
					handleFactoryComponents(buildable, EFactoryConnectionConnector::FCC_CONVEYOR, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				auto storageContainer = Cast<AFGBuildableStorage>(owner);
				buildable = storageContainer;

				if (storageContainer)
				{
					handleContainerComponents(
						storageContainer,
						EFactoryConnectionConnector::FCC_CONVEYOR,
						storageContainer->GetStorageInventory(),
						collectSettings,
						inputComponents,
						outputComponents
						);
				}
			}

			if (!buildable)
			{
				auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner);
				buildable = cargoPlatform;

				if (cargoPlatform)
				{
					handleTrainPlatformCargo(cargoPlatform, EFactoryConnectionConnector::FCC_CONVEYOR, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				auto dockingStation = Cast<AFGBuildableDockingStation>(owner);
				buildable = dockingStation;

				if (dockingStation)
				{
					handleContainerComponents(
						dockingStation,
						EFactoryConnectionConnector::FCC_CONVEYOR,
						dockingStation->GetInventory(),
						collectSettings,
						inputComponents,
						outputComponents,
						[](class UFGFactoryConnectionComponent* component)
						{
							return !component->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase);
						}
						);
				}
			}

			if (buildable)
			{
				if (inputComponents.Num() == 1 && outputComponents.Num() == 1)
				{
					collectSettings.GetConnected().Add(buildable);

					collectSettings.SetConnector(inputComponents.begin().Key()->GetConnection());

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" skipped"));

					collectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), inputComponents.begin().Value()));

					// Check if anything can still POSSIBLY flow through
					if (collectSettings.GetCurrentFilter().allowedFiltered && !collectSettings.GetCurrentFilter().allowedItems.Num())
					{
						// Is filtered and has no "pass-through" item. Stop crawling
						return;
					}

					continue;
				}

				if (inputComponents.Num() == 0)
				{
					// Nothing is being inputed. Bail
					EC_LOG_Error_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" has no input"));
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					for (auto connectionEntry : inputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.Key;

						if (!connection->IsConnected())
						{
							continue;
						}

						// if (dockingStation && connection->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase))
						// {
						// 	continue;
						// }

						auto previousLimit = collectSettings.GetLimitedThroughput();
						auto newIndent = collectSettings.GetIndent() + TEXT("    ");
						auto newLevel = collectSettings.GetLevel() + 1;

						FComponentFilter tempComponentFilter = FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), connectionEntry.Value);

						CollectSettingsWrapper tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection());
						tempCollectSettings.SetLimitedThroughputPtr(&previousLimit);
						tempCollectSettings.SetIndentPtr(&newIndent);
						tempCollectSettings.SetLevelPtr(&newLevel);
						tempCollectSettings.SetCurrentFilterPtr(&tempComponentFilter);

						collectInput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (firstConnection)
						{
							limitedThroughput = previousLimit;
							firstConnection = false;
						}
						else
						{
							limitedThroughput += previousLimit;
						}
					}

					collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), limitedThroughput));

					for (auto connectionEntry : outputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.Key;

						if (!connection->IsConnected())
						{
							continue;
						}

						float previousLimit = 0;
						float discountedInput = 0;
					}
				}

				collectSettings.GetConnected().Add(buildable);
			}
		}

		if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
		{
		}

		{
			const auto nuclearGenerator = Cast<AFGBuildableGeneratorNuclear>(owner);
			if (nuclearGenerator &&
				(nuclearGenerator->HasPower() || Has_EMachineStatusIncludeType(collectSettings. GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!nuclearGenerator->IsProductionPaused() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Paused)))
			{
				for (auto item : AEfficiencyCheckerLogic::singleton->nuclearWasteItemDescriptors)
				{
					collectSettings.GetInjectedItems().Add(item);

					collectSettings.GetInjectedInput().FindOrAdd(item) += 0.2;
				}

				collectSettings.GetConnected().Add(nuclearGenerator);

				return;
			}
		}

		if (AEfficiencyCheckerLogic::singleton->baseBuildableFactorySimpleProducerClass &&
			owner->IsA(AEfficiencyCheckerLogic::singleton->baseBuildableFactorySimpleProducerClass))
		{
			TSubclassOf<UFGItemDescriptor> itemType = FReflectionHelper::GetObjectPropertyValue<UClass>(owner, TEXT("mItemType"));
			auto timeToProduceItem = FReflectionHelper::GetPropertyValue<FFloatProperty>(owner, TEXT("mTimeToProduceItem"));

			if (timeToProduceItem && itemType)
			{
				collectSettings.GetInjectedItems().Add(itemType);

				collectSettings.GetInjectedInput().FindOrAdd(itemType) += 60 / timeToProduceItem;
			}

			collectSettings.GetConnected().Add(Cast<AFGBuildable>(owner));

			return;
		}
	}
}

void AEfficiencyCheckerLogic2::collectOutput(ICollectSettings& collectSettings)
{
	TSet<TSubclassOf<UFGItemDescriptor>> injectedItems = collectSettings.GetInjectedItems();

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

		if (injectedItems.Num())
		{
			bool unusedItems = false;

			for (auto item : injectedItems)
			{
				if (!AEfficiencyCheckerLogic::actorContainsItem(collectSettings.GetSeenActors(), owner, item))
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
			if (AEfficiencyCheckerLogic::containsActor(collectSettings.GetSeenActors(), owner))
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
			*fullClassName
			);

		{
			const auto manufacturer = Cast<AFGBuildableManufacturer>(owner);

			if (manufacturer)
			{
				handleManufacturer(
					manufacturer,
					collectSettings,
					false,
					true
					);

				return;
			}
		}

		if (collectSettings.GetResourceForm() == EResourceForm::RF_SOLID)
		{
			const auto conveyor = Cast<AFGBuildableConveyorBase>(owner);
			if (conveyor)
			{
				AEfficiencyCheckerLogic::addAllItemsToActor(collectSettings.GetSeenActors(), conveyor, injectedItems);

				collectSettings.GetConnected().Add(conveyor);

				collectSettings.SetConnector(conveyor->GetConnection1()->GetConnection());

				collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), conveyor->GetSpeed() / 2));

				EC_LOG_Display_Condition(*collectSettings.GetIndent(), *conveyor->GetName(), TEXT(" limited at "), collectSettings.GetLimitedThroughput(), TEXT(" items/minute"));

				continue;
			}

			AFGBuildable* buildable = nullptr;

			TMap<UFGFactoryConnectionComponent*, FComponentFilter> inputComponents, outputComponents;

			if (!buildable &&
				(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterInputClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterInputClass) ||
					AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterOutputClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterOutputClass)))
			{
				auto undergroundBelt = Cast<AFGBuildableStorage>(owner);
				buildable = undergroundBelt;

				if (undergroundBelt)
				{
					handleUndergroundBeltsComponents(undergroundBelt, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!AEfficiencyCheckerConfiguration::configuration.ignoreStorageTeleporter &&
				!buildable && AEfficiencyCheckerLogic::singleton->baseStorageTeleporterClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseStorageTeleporterClass))
			{
				auto storageTeleporter = Cast<AFGBuildableFactory>(owner);
				buildable = storageTeleporter;

				if (storageTeleporter)
				{
					handleStorageTeleporter(storageTeleporter, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!buildable && AEfficiencyCheckerLogic::singleton->baseModularLoadBalancerClass && owner->IsA(AEfficiencyCheckerLogic::singleton->baseModularLoadBalancerClass))
			{
				auto modularLoadBalancer = FReflectionHelper::GetObjectPropertyValue<AFGBuildableFactory>(owner, TEXT("GroupLeader"));
				buildable = modularLoadBalancer;

				if (modularLoadBalancer)
				{
					handleModularLoadBalancerComponents(modularLoadBalancer, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				buildable = Cast<AFGBuildableConveyorAttachment>(owner);
				if (buildable)
				{
					handleFactoryComponents(buildable, EFactoryConnectionConnector::FCC_CONVEYOR, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				auto storageContainer = Cast<AFGBuildableStorage>(owner);
				buildable = storageContainer;

				if (storageContainer)
				{
					handleContainerComponents(
						storageContainer,
						EFactoryConnectionConnector::FCC_CONVEYOR,
						storageContainer->GetStorageInventory(),
						collectSettings,
						inputComponents,
						outputComponents
						);
				}
			}

			if (!buildable)
			{
				auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner);
				buildable = cargoPlatform;

				if (cargoPlatform)
				{
					handleTrainPlatformCargo(cargoPlatform, EFactoryConnectionConnector::FCC_CONVEYOR, collectSettings, inputComponents, outputComponents);
				}
			}

			if (!buildable)
			{
				auto dockingStation = Cast<AFGBuildableDockingStation>(owner);
				buildable = dockingStation;

				if (dockingStation)
				{
					handleContainerComponents(
						dockingStation,
						EFactoryConnectionConnector::FCC_CONVEYOR,
						dockingStation->GetInventory(),
						collectSettings,
						inputComponents,
						outputComponents,
						[](class UFGFactoryConnectionComponent* component)
						{
							return !component->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase);
						}
						);
				}
			}

			if (buildable)
			{
				if (inputComponents.Num() == 1 && outputComponents.Num() == 1)
				{
					collectSettings.GetConnected().Add(buildable);

					collectSettings.SetConnector(outputComponents.begin().Key()->GetConnection());

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" skipped"));

					injectedItems = outputComponents.begin().Value().filterItems(injectedItems);

					collectSettings.SetCurrentFilter(FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), outputComponents.begin().Value()));

					// Check if anything can still POSSIBLY flow through
					if (!injectedItems.Num())
					{
						// No item is being inject. Can't go any further
						return;
					}

					continue;
				}

				if (outputComponents.Num() == 0)
				{
					// Nothing is being outputed. Bail
					EC_LOG_Error_Condition(*collectSettings.GetIndent(), *buildable->GetName(), TEXT(" has no output"));
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					for (auto connectionEntry : outputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.Key;

						if (!connection->IsConnected())
						{
							continue;
						}

						CollectSettingsWrapper tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(collectSettings.GetLimitedThroughput(), true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("    "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetCurrentFilter(connectionEntry.Value);

						collectOutput(tempCollectSettings);
						
						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (firstConnection)
						{
							collectSettings.GetLimitedThroughput() = tempCollectSettings.GetLimitedThroughput();
							firstConnection = false;
						}
						else
						{
							collectSettings.GetLimitedThroughput() += tempCollectSettings.GetLimitedThroughput();
						}
					}

					collectSettings.SetLimitedThroughput(FMath::Min(collectSettings.GetLimitedThroughput(), limitedThroughput));

					for (auto connectionEntry : inputComponents)
					{
						if (collectSettings.GetTimeout() < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							collectSettings.SetOverflow(true);
							return;
						}

						auto connection = connectionEntry.Key;

						if (!connection->IsConnected())
						{
							continue;
						}

						FComponentFilter tempComponentFilter = FComponentFilter::combineFilters(collectSettings.GetCurrentFilter(), connectionEntry.Value);

						CollectSettingsWrapper tempCollectSettings(collectSettings);
						tempCollectSettings.SetConnector(connection->GetConnection(), true);
						tempCollectSettings.SetLimitedThroughput(0, true);
						tempCollectSettings.SetIndent(collectSettings.GetIndent() + TEXT("    "), true);
						tempCollectSettings.SetLevel(collectSettings.GetLevel() + 1, true);
						tempCollectSettings.SetCurrentFilterPtr(&tempComponentFilter);
						tempCollectSettings.SetRequiredOutput(TMAP_ITEM_INT32(), true);
						tempCollectSettings.SetSeenActors(collectSettings.GetSeenActors(), true);
						tempCollectSettings.SetInjectedItems(injectedItems, true);

						collectInput(tempCollectSettings);

						if (collectSettings.GetOverflow())
						{
							return;
						}

						if (tempCollectSettings.GetRequiredOutputTotal() > 0)
						{
							EC_LOG_Display_Condition(*tempCollectSettings.GetIndent(), TEXT("Discounting "), tempCollectSettings.GetRequiredOutputTotal(), TEXT(" items/minute"));

							for (auto entry : tempCollectSettings.GetRequiredOutput())
							{
								collectSettings.GetRequiredOutput().FindOrAdd(entry.Key) -= entry.Value;
							}
						}
					}
				}
			}
		}
	}
}

void AEfficiencyCheckerLogic2::handleManufacturer
(
	class AFGBuildableManufacturer* const manufacturer,
	ICollectSettings& collectSettings,
	bool collectInjectedInput,
	bool collectRequiredOutput
)
{
	if ((manufacturer->HasPower() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Unpowered)) &&
		(!manufacturer->IsProductionPaused() || Has_EMachineStatusIncludeType(collectSettings.GetMachineStatusIncludeType(), EMachineStatusIncludeType::MSIT_Paused)))
	{
		const auto recipeClass = manufacturer->GetCurrentRecipe();

		collectSettings.GetConnected().Add(manufacturer);

		if (recipeClass)
		{
			if (collectInjectedInput)
			{
				auto products = UFGRecipe::GetProducts(recipeClass)
					.FilterByPredicate(
						[&collectSettings](const FItemAmount& item)
						{
							return UFGItemDescriptor::GetForm(item.ItemClass) == collectSettings.GetResourceForm();
						}
						);

				if (products.Num())
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
									index1 = FCString::Atoi(*m1.GetCaptureGroup(1));
								}

								FRegexMatcher m2(AEfficiencyCheckerLogic::indexPattern, y);
								if (m2.FindNext())
								{
									index2 = FCString::Atoi(*m2.GetCaptureGroup(1));
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

					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Item amount = "), item.Amount);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Current potential = "), manufacturer->GetCurrentPotential());
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Pending potential = "), manufacturer->GetPendingPotential());
					EC_LOG_Display_Condition(
						*collectSettings.GetIndent(),
						TEXT("Production cycle time = "),
						manufacturer->CalcProductionCycleTimeForPotential(manufacturer->GetPendingPotential())
						);
					EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Recipe duration = "), UFGRecipe::GetManufacturingDuration(recipeClass));

					float itemAmountPerMinute = item.Amount * manufacturer->GetPendingPotential() * 60 /
						manufacturer->GetDefaultProductionCycleTime();

					if (collectSettings.GetResourceForm() == EResourceForm::RF_LIQUID || collectSettings.GetResourceForm() == EResourceForm::RF_GAS)
					{
						itemAmountPerMinute /= 1000;
					}

					EC_LOG_Display_Condition(
						/**getTimeStamp(),*/
						*collectSettings.GetIndent(),
						*manufacturer->GetName(),
						TEXT(" produces "),
						itemAmountPerMinute,
						TEXT(" "),
						*UFGItemDescriptor::GetItemName(item.ItemClass).ToString(),
						TEXT("/minute")
						);

					collectSettings.GetInjectedItems().Add(item.ItemClass);

					if (!collectSettings.GetCustomInjectedInput())
					{
						collectSettings.GetInjectedInput().FindOrAdd(item.ItemClass) += itemAmountPerMinute;
					}
				}
			}

			if (collectRequiredOutput)
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

					if (!collectSettings.GetInjectedItems().Contains(item.ItemClass) ||
						collectSettings.GetSeenActors().Contains(manufacturer) && collectSettings.GetSeenActors()[manufacturer].Contains(item.ItemClass))
					{
						continue;
					}

					if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
					{
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Item amount = "), item.Amount);
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Current potential = "), manufacturer->GetCurrentPotential());
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Pending potential = "), manufacturer->GetPendingPotential());
						EC_LOG_Display(
							*collectSettings.GetIndent(),
							TEXT("Production cycle time = "),
							manufacturer->CalcProductionCycleTimeForPotential(manufacturer->GetPendingPotential())
							);
						EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Recipe duration = "), UFGRecipe::GetManufacturingDuration(recipeClass));
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
						*manufacturer->GetName(),
						TEXT(" consumes "),
						itemAmountPerMinute,
						TEXT(" "),
						*UFGItemDescriptor::GetItemName(item.ItemClass).ToString(),
						TEXT("/minute")
						);

					if (!collectSettings.GetCustomRequiredOutput())
					{
						collectSettings.GetRequiredOutput().FindOrAdd(item.ItemClass) += itemAmountPerMinute;
					}

					collectSettings.GetSeenActors().FindOrAdd(manufacturer).Add(item.ItemClass);
				}
			}
		}
	}
}

void AEfficiencyCheckerLogic2::handleExtractor(AFGBuildableResourceExtractor* extractor, ICollectSettings& collectSettings)
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
			TEXT("Extraction Speed Multiplier = "),
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

		EC_LOG_Display_Condition(*collectSettings.GetIndent(), TEXT("Resource name = "), *UFGItemDescriptor::GetItemName(item).ToString());

		collectSettings.GetInjectedItems().Add(item);

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Current potential = "), extractor->GetCurrentPotential());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Pending potential = "), extractor->GetPendingPotential());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Default cycle time = "), extractor->GetDefaultExtractCycleTime());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Production cycle time = "), extractor->GetProductionCycleTime());
			EC_LOG_Display(
				*collectSettings.GetIndent(),
				TEXT("Production cycle time for potential = "),
				extractor->CalcProductionCycleTimeForPotential(extractor->GetPendingPotential())
				);
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Items per cycle converted = "), extractor->GetNumExtractedItemsPerCycleConverted());
			EC_LOG_Display(*collectSettings.GetIndent(), TEXT("Items per cycle = "), extractor->GetNumExtractedItemsPerCycle());
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
			collectSettings.GetInjectedInput().FindOrAdd(item) += itemAmountPerMinute;
		}

		collectSettings.GetConnected().Add(extractor);
	}
}

void AEfficiencyCheckerLogic2::handleFactoryComponents
(
	class AFGBuildable* buildable,
	EFactoryConnectionConnector connectorType,
	TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	TMap<class UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	const std::function<bool (class UFGFactoryConnectionComponent*)>& filter
)
{
	TArray<UFGFactoryConnectionComponent*> tempComponents;
	buildable->GetComponents(tempComponents);

	for (auto component : tempComponents.FilterByPredicate(
		     [connectorType,filter](UFGFactoryConnectionComponent* connection)
		     {
			     return connection->IsConnected() && connection->GetConnector() == connectorType && filter(connection);
		     }
		     )
		)
	{
		if (!inputComponents.Contains(component) && component->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
		{
			inputComponents.Add(component);
		}
		else if (!outputComponents.Contains(component) && component->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
		{
			outputComponents.Add(component);
		}
	}
}

void AEfficiencyCheckerLogic2::handleUndergroundBeltsComponents
(
	AFGBuildableStorage* undergroundBelt,
	ICollectSettings& collectSettings,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
)
{
	handleFactoryComponents(undergroundBelt, EFactoryConnectionConnector::FCC_CONVEYOR, inputComponents, outputComponents);

	if (AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterInputClass &&
		undergroundBelt->IsA(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterInputClass))
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
				collectSettings.GetSeenActors().Add(outputUndergroundBelt);
				collectSettings.GetConnected().Add(outputUndergroundBelt);

				handleFactoryComponents(
					outputUndergroundBelt,
					EFactoryConnectionConnector::FCC_CONVEYOR,
					inputComponents,
					outputComponents,
					[outputComponents](UFGFactoryConnectionComponent* connection)
					{
						return !outputComponents.Contains(connection) && connection->IsConnected() && connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT;
						// Is output connection
					}
					);
			}
		}
	}
	else if (AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterOutputClass &&
		undergroundBelt->IsA(AEfficiencyCheckerLogic::singleton->baseUndergroundSplitterOutputClass))
	{
		for (auto inputUndergroundBelt : AEfficiencyCheckerLogic::singleton->allUndergroundInputBelts)
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

			collectSettings.GetSeenActors().Add(inputUndergroundBelt);
			collectSettings.GetConnected().Add(inputUndergroundBelt);

			handleFactoryComponents(
				inputUndergroundBelt,
				EFactoryConnectionConnector::FCC_CONVEYOR,
				inputComponents,
				outputComponents,
				[outputComponents](UFGFactoryConnectionComponent* connection)
				{
					return !outputComponents.Contains(connection) && connection->IsConnected() && connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT;
					// Is output connection
				}
				);
		}
	}
}

void AEfficiencyCheckerLogic2::handleContainerComponents
(
	AFGBuildable* buildable,
	EFactoryConnectionConnector connectorType,
	UFGInventoryComponent* inventory,
	ICollectSettings& collectSettings,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents,
	const std::function<bool (class UFGFactoryConnectionComponent*)>& filter
)
{
	handleFactoryComponents(buildable, connectorType, inputComponents, outputComponents, filter);

	TArray<FInventoryStack> stacks;

	inventory->GetInventoryStacks(stacks);

	for (auto stack : stacks)
	{
		if (!collectSettings.GetCurrentFilter().itemIsAllowed(stack.Item.GetItemClass()))
		{
			continue;
		}

		collectSettings.GetInjectedItems().Add(stack.Item.GetItemClass());
	}
}

void AEfficiencyCheckerLogic2::handleTrainPlatformCargo
(
	AFGBuildableTrainPlatformCargo* cargoPlatform,
	EFactoryConnectionConnector connectorType,
	ICollectSettings& collectSettings,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
)
{
	handleContainerComponents(cargoPlatform, connectorType, cargoPlatform->GetInventory(), collectSettings, inputComponents, outputComponents);

	auto trackId = cargoPlatform->GetTrackGraphID();

	auto railroadSubsystem = AFGRailroadSubsystem::Get(cargoPlatform->GetWorld());

	// Determine offsets from all the connected stations
	std::set<int> stationOffsets;
	TSet<AFGBuildableRailroadStation*> destinationStations;

	for (auto i = 0; i <= 1; i++)
	{
		auto offsetDistance = 1;

		TSet<AFGBuildableTrainPlatform*> seenPlatforms;

		for (auto connectedPlatform = cargoPlatform->GetConnectedPlatformInDirectionOf(i);
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

		for (auto stop : stops)
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

		for (auto stop : stops)
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
					if (!stopCargo || stopCargo == cargoPlatform)
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

					collectSettings.GetSeenActors().Add(stopCargo);
					collectSettings.GetConnected().Add(stopCargo);

					for (auto component : stopCargo->GetConnectionComponents())
					{
						if (component->GetDirection() == EFactoryConnectionDirection::FCD_INPUT && stopCargo->GetIsInLoadMode())
						{
							inputComponents.Add(component);
						}
						if (component->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT && !stopCargo->GetIsInLoadMode())
						{
							outputComponents.Add(component);
						}
					}
				}
			}
		}
	}
}

void AEfficiencyCheckerLogic2::handleStorageTeleporter
(
	AFGBuildable* storageTeleporter,
	ICollectSettings& collectSettings,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
)
{
	// Find all others of the same type
	auto currentStorageID = FReflectionHelper::GetPropertyValue<FStrProperty>(storageTeleporter, TEXT("StorageID"));

	handleFactoryComponents(storageTeleporter, EFactoryConnectionConnector::FCC_CONVEYOR, inputComponents, outputComponents);

	FScopeLock ScopeLock(&AEfficiencyCheckerLogic::singleton->eclCritical);

	for (auto testTeleporter : AEfficiencyCheckerLogic::singleton->allTeleporters)
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

		collectSettings.GetSeenActors().Add(testTeleporter);
		collectSettings.GetConnected().Add(testTeleporter);

		handleFactoryComponents(testTeleporter, EFactoryConnectionConnector::FCC_CONVEYOR, inputComponents, outputComponents);
	}
}

void AEfficiencyCheckerLogic2::handleModularLoadBalancerComponents
(
	AFGBuildableFactory* modularLoadBalancer,
	ICollectSettings& collectSettings,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& inputComponents,
	TMap<UFGFactoryConnectionComponent*, FComponentFilter>& outputComponents
)
{
	auto arrayProperty = CastField<FArrayProperty>(modularLoadBalancer->GetClass()->FindPropertyByName(TEXT("mGroupModules")));
	if (!arrayProperty)
	{
		return;
	}

	if (arrayProperty)
	{
		FScriptArrayHelper arrayHelper(arrayProperty, arrayProperty->ContainerPtrToValuePtr<void>(modularLoadBalancer));

		auto arrayWeakObjectProperty = CastField<FWeakObjectProperty>(arrayProperty->Inner);

		for (auto x = 0; x < arrayHelper.Num(); x++)
		{
			void* ObjectContainer = arrayHelper.GetRawPtr(x);
			auto loadBalancerModule = Cast<AFGBuildableFactory>(arrayWeakObjectProperty->GetObjectPropertyValue(ObjectContainer));
			if (!loadBalancerModule)
			{
				continue;
			}

			collectSettings.GetSeenActors().Add(loadBalancerModule);

			handleFactoryComponents(loadBalancerModule, EFactoryConnectionConnector::FCC_CONVEYOR, inputComponents, outputComponents);
		}
	}
}

#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
