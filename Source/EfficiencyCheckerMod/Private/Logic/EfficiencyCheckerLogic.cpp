﻿// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo
// ReSharper disable IdentifierTypo

#include "Logic/EfficiencyCheckerLogic.h"
#include "Util/EfficiencyCheckerOptimize.h"
#include "Util/Logging.h"

#include "AkAudioEvent.h"
#include "Animation/AnimSequence.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorNuclear.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "EfficiencyCheckerRCO.h"
#include "EfficiencyChecker_ConfigStruct.h"
#include "Equipment/FGEquipment.h"
#include "Equipment/FGEquipmentAttachment.h"
#include "FGConnectionComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "FGItemCategory.h"
#include "FGPipeConnectionComponent.h"
#include "FGRailroadSubsystem.h"
#include "FGRailroadTimeTable.h"
#include "FGTrain.h"
#include "FGTrainStationIdentifier.h"
#include "Patching/NativeHookManager.h"
#include "Reflection/ReflectionHelper.h"
#include "Resources/FGEquipmentDescriptor.h"
#include "Animation/AimOffsetBlendSpace.h"

#include <map>
#include <set>

#include "EfficiencyCheckerBuilding.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildablePipeline.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

// TSet<TSubclassOf<UFGItemDescriptor>> AEfficiencyCheckerLogic::nuclearWasteItemDescriptors;
// TSet<TSubclassOf<UFGItemDescriptor>> AEfficiencyCheckerLogic::noneItemDescriptors;
// TSet<TSubclassOf<UFGItemDescriptor>> AEfficiencyCheckerLogic::wildCardItemDescriptors;
// TSet<TSubclassOf<UFGItemDescriptor>> AEfficiencyCheckerLogic::anyUndefinedItemDescriptors;
// TSet<TSubclassOf<UFGItemDescriptor>> AEfficiencyCheckerLogic::overflowItemDescriptors;
//
// FCriticalSection AEfficiencyCheckerLogic::eclCritical;

AEfficiencyCheckerLogic* AEfficiencyCheckerLogic::singleton = nullptr;
FEfficiencyChecker_ConfigStruct AEfficiencyCheckerLogic::configuration;

// TSet<class AEfficiencyCheckerBuilding*> AEfficiencyCheckerLogic::allEfficiencyBuildings;

inline FString getEnumItemName(const TCHAR* name, int value)
{
	FString valueStr;

	auto MyEnum = FindObject<UEnum>(ANY_PACKAGE, name);
	if (MyEnum)
	{
		MyEnum->AddToRoot();

		valueStr = MyEnum->GetDisplayNameTextByValue(value).ToString();
	}
	else
	{
		valueStr = TEXT("(Unknown)");
	}

	return FString::Printf(TEXT("%s (%d)"), *valueStr, value);
}

void AEfficiencyCheckerLogic::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Terminate();
}

void AEfficiencyCheckerLogic::Initialize
(
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_noneItemDescriptors,
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_wildcardItemDescriptors,
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_anyUndefinedItemDescriptors,
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_overflowItemDescriptors,
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_nuclearWasteItemDescriptors
)
{
	singleton = this;

	noneItemDescriptors = in_noneItemDescriptors;
	wildCardItemDescriptors = in_wildcardItemDescriptors;
	anyUndefinedItemDescriptors = in_anyUndefinedItemDescriptors;
	overflowItemDescriptors = in_overflowItemDescriptors;
	nuclearWasteItemDescriptors = in_nuclearWasteItemDescriptors;

	removeEffiencyBuildingDelegate.BindDynamic(this, &AEfficiencyCheckerLogic::removeEfficiencyBuilding);
	removeBeltDelegate.BindDynamic(this, &AEfficiencyCheckerLogic::removeBelt);
	removePipeDelegate.BindDynamic(this, &AEfficiencyCheckerLogic::removePipe);
	removeTeleporterDelegate.BindDynamic(this, &AEfficiencyCheckerLogic::removeTeleporter);
	removeUndergroundInputBeltDelegate.BindDynamic(this, &AEfficiencyCheckerLogic::removeUndergroundInputBelt);

	auto subsystem = AFGBuildableSubsystem::Get(this);

	if (subsystem)
	{
		FScopeLock ScopeLock(&eclCritical);

		TArray<AActor*> allBuildables;
		UGameplayStatics::GetAllActorsOfClass(subsystem->GetWorld(), AFGBuildable::StaticClass(), allBuildables);

		for (auto buildableActor : allBuildables)
		{
			IsValidBuildable(Cast<AFGBuildable>(buildableActor));
		}

		// auto gameMode = Cast<AFGGameMode>(UGameplayStatics::GetGameMode(subsystem->GetWorld()));
		// if (gameMode)
		// {
		// 	gameMode->RegisterRemoteCallObjectClass(UEfficiencyCheckerRCO::StaticClass());
		// }
	}
}

void AEfficiencyCheckerLogic::Terminate()
{
	FScopeLock ScopeLock(&eclCritical);
	allEfficiencyBuildings.Empty();
	allBelts.Empty();
	allPipes.Empty();
	allTeleporters.Empty();
	allUndergroundInputBelts.Empty();

	singleton = nullptr;
}

bool AEfficiencyCheckerLogic::containsActor(const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors, AActor* actor)
{
	return seenActors.find(actor) != seenActors.end();
}

bool AEfficiencyCheckerLogic::actorContainsItem
(
	const std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors,
	AActor* actor,
	const TSubclassOf<UFGItemDescriptor>& item
)
{
	auto it = seenActors.find(actor);
	if (it == seenActors.end())
	{
		return false;
	}

	return it->second.Contains(item);
}

void AEfficiencyCheckerLogic::addAllItemsToActor
(
	std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors,
	AActor* actor,
	const TSet<TSubclassOf<UFGItemDescriptor>>& items
)
{
	// Ensure the actor exists, even with an empty list
	TSet<TSubclassOf<UFGItemDescriptor>>& itemsSet = seenActors[actor];

	for (auto item : items)
	{
		itemsSet.Add(item);
	}
}

void AEfficiencyCheckerLogic::collectInput
(
	EResourceForm resourceForm,
	bool customInjectedInput,
	class UFGConnectionComponent* connector,
	float& out_injectedInput,
	float& out_limitedThroughput,
	TSet<AActor*>& seenActors,
	TSet<class AFGBuildable*>& connected,
	TSet<TSubclassOf<UFGItemDescriptor>>& out_injectedItems,
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_restrictItems,
	class AFGBuildableSubsystem* buildableSubsystem,
	int level,
	bool& overflow,
	const FString& indent,
	const time_t& timeout,
	int machineStatusIncludeType
)
{
	TSet<TSubclassOf<UFGItemDescriptor>> restrictItems = in_restrictItems;

	for (;;)
	{
		if (!connector)
		{
			return;
		}

		auto owner = connector->GetOwner();

		if (!owner || seenActors.Contains(owner))
		{
			return;
		}

		if (timeout < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout!"));

			overflow = true;
			return;
		}

		const auto fullClassName = GetPathNameSafe(owner->GetClass());

		if (level > 100)
		{
			EC_LOG_Error_Condition(
				FUNCTIONSTR TEXT(": level is too deep: "),
				level,
				TEXT("; "),
				*owner->GetName(),
				TEXT(" / "),
				*fullClassName
				);

			overflow = true;

			return;
		}

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*indent,
			TEXT("collectInput at level "),
			level,
			TEXT(": "),
			*owner->GetName(),
			TEXT(" / "),
			*fullClassName
			);

		seenActors.Add(owner);

		{
			const auto manufacturer = Cast<AFGBuildableManufacturer>(owner);
			if (manufacturer &&
				(manufacturer->HasPower() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!manufacturer->IsProductionPaused() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Paused)))
			{
				const auto recipeClass = manufacturer->GetCurrentRecipe();

				connected.Add(manufacturer);

				if (recipeClass)
				{
					auto products = UFGRecipe::GetProducts(recipeClass)
						.FilterByPredicate(
							[resourceForm](const FItemAmount& item)
							{
								return UFGItemDescriptor::GetForm(item.ItemClass) == resourceForm;
							}
							);

					if (products.Num())
					{
						int outputIndex = 0;

						if (products.Num() > 1)
						{
							TArray<FString> names;

							auto isBelt = Cast<UFGFactoryConnectionComponent>(connector) != nullptr;
							auto isPipe = Cast<UFGPipeConnectionComponent>(connector) != nullptr;

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
									return x.Compare(y, ESearchCase::IgnoreCase) < 0;
								}
								);

							outputIndex = names.Find(connector->GetName());
						}

						auto item = outputIndex > 0 ? products[outputIndex] : products[0];

						if (!restrictItems.Contains(item.ItemClass))
						{
							return;
						}

						out_injectedItems.Add(item.ItemClass);

						EC_LOG_Display_Condition(*indent, TEXT("Item amount = "), item.Amount);
						EC_LOG_Display_Condition(*indent, TEXT("Current potential = "), manufacturer->GetCurrentPotential());
						EC_LOG_Display_Condition(*indent, TEXT("Pending potential = "), manufacturer->GetPendingPotential());
						EC_LOG_Display_Condition(
							*indent,
							TEXT("Production cycle time = "),
							manufacturer->CalcProductionCycleTimeForPotential(manufacturer->GetPendingPotential())
							);
						EC_LOG_Display_Condition(*indent, TEXT("Recipe duration = "), UFGRecipe::GetManufacturingDuration(recipeClass));

						float itemAmountPerMinute = item.Amount * manufacturer->GetPendingPotential() * 60 /
							manufacturer->GetDefaultProductionCycleTime();

						if (resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS)
						{
							itemAmountPerMinute /= 1000;
						}

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
							*manufacturer->GetName(),
							TEXT(" produces "),
							itemAmountPerMinute,
							TEXT(" "),
							*UFGItemDescriptor::GetItemName(item.ItemClass).ToString(),
							TEXT("/minute")
							);

						if (!customInjectedInput)
						{
							out_injectedInput += itemAmountPerMinute;
						}
					}
				}

				return;
			}
		}

		{
			const auto extractor = Cast<AFGBuildableResourceExtractor>(owner);
			if (extractor &&
				(extractor->HasPower() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!extractor->IsProductionPaused() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Paused)))
			{
				TSubclassOf<UFGItemDescriptor> item;

				const auto resource = extractor->GetExtractableResource();

				auto speedMultiplier = resource ? resource->GetExtractionSpeedMultiplier() : 1;

				EC_LOG_Display_Condition(
					/**getTimeStamp(),*/
					*indent,
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
							*indent,
							TEXT("Extractable resource is null")
							);
					}
				}

				if (!item)
				{
					item = extractor->GetOutputInventory()->GetAllowedItemOnIndex(0);
				}

				if (!item || !restrictItems.Contains(item))
				{
					return;
				}

				EC_LOG_Display_Condition(*indent, TEXT("Resource name = "), *UFGItemDescriptor::GetItemName(item).ToString());

				out_injectedItems.Add(item);

				if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
				{
					EC_LOG_Display(*indent, TEXT("Current potential = "), extractor->GetCurrentPotential());
					EC_LOG_Display(*indent, TEXT("Pending potential = "), extractor->GetPendingPotential());
					EC_LOG_Display(*indent, TEXT("Default cycle time = "), extractor->GetDefaultExtractCycleTime());
					EC_LOG_Display(*indent, TEXT("Production cycle time = "), extractor->GetProductionCycleTime());
					EC_LOG_Display(*indent, TEXT("Production cycle time for potential = "), extractor->CalcProductionCycleTimeForPotential(extractor->GetPendingPotential()));
					EC_LOG_Display(*indent, TEXT("Items per cycle converted = "), extractor->GetNumExtractedItemsPerCycleConverted());
					EC_LOG_Display(*indent, TEXT("Items per cycle = "), extractor->GetNumExtractedItemsPerCycle());
				}

				float itemAmountPerMinute;

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

					if (resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS)
					{
						itemAmountPerMinute /= 1000;
					}
				}

				EC_LOG_Display_Condition(
					/**getTimeStamp(),*/
					*indent,
					*extractor->GetName(),
					TEXT(" extracts "),
					itemAmountPerMinute,
					TEXT(" "),
					*UFGItemDescriptor::GetItemName(item).ToString(),
					TEXT("/minute")
					);

				if (!customInjectedInput)
				{
					out_injectedInput += itemAmountPerMinute;
				}

				connected.Add(extractor);

				return;
			}
		}

		if (resourceForm == EResourceForm::RF_SOLID)
		{
			const auto conveyor = Cast<AFGBuildableConveyorBase>(owner);
			if (conveyor)
			{
				// The innitial limit for a belt is its own speed
				// out_limitedThroughput = conveyor->GetSpeed() / 2;

				// const auto conveyorInput = conveyor->GetConnection0();
				// if (conveyorInput && conveyorInput->IsConnected())
				// {
				//     float previousLimit = out_limitedThroughput;
				//     collectInput(
				//         resourceForm,
				//         customInjectedInput,
				//         conveyorInput->GetConnection(),
				//         out_injectedInput,
				//         previousLimit,
				//         seenActors,
				//         connected,
				//         out_injectedItems,
				//         restrictItems,
				//         buildableSubsystem,
				//         level + 1,
				//         indent + TEXT("    ")
				//         );
				//
				//     out_limitedThroughput = FMath::Min(out_limitedThroughput, previousLimit);
				// }

				connected.Add(conveyor);

				connector = conveyor->GetConnection0()->GetConnection();

				out_limitedThroughput = FMath::Min(out_limitedThroughput, conveyor->GetSpeed() / 2);

				EC_LOG_Display_Condition(*indent, *conveyor->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" items/minute"));

				continue;
			}

			AFGBuildableStorage* storageContainer = nullptr;
			AFGBuildableTrainPlatformCargo* cargoPlatform = nullptr;
			AFGBuildableConveyorAttachment* conveyorAttachment = nullptr;
			AFGBuildableDockingStation* dockingStation = nullptr;
			AFGBuildableFactory* storageTeleporter = nullptr;
			AFGBuildableStorage* undergroundBelt = nullptr;

			AFGBuildable* buildable = conveyorAttachment = Cast<AFGBuildableConveyorAttachment>(owner);

			if (!buildable && (fullClassName.EndsWith("/UndergroundBelts/Build/Build_UndergroundSplitterInput.Build_UndergroundSplitterInput_C") ||
				fullClassName.EndsWith("/UndergroundBelts/Build/Build_UndergroundSplitterOutput.Build_UndergroundSplitterOutput_C")))
			{
				buildable = undergroundBelt = Cast<AFGBuildableStorage>(owner);
			}

			if (!buildable)
			{
				buildable = storageContainer = Cast<AFGBuildableStorage>(owner);
			}

			if (!buildable)
			{
				cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner);
				if (cargoPlatform)
				{
					buildable = cargoPlatform;

					TArray<FInventoryStack> stacks;

					cargoPlatform->GetInventory()->GetInventoryStacks(stacks);

					for (auto stack : stacks)
					{
						if (!restrictItems.Contains(stack.Item.GetItemClass()))
						{
							continue;
						}

						out_injectedItems.Add(stack.Item.GetItemClass());
					}
				}
			}

			if (!buildable)
			{
				dockingStation = Cast<AFGBuildableDockingStation>(owner);
				if (dockingStation)
				{
					buildable = dockingStation;

					TArray<FInventoryStack> stacks;

					dockingStation->GetInventory()->GetInventoryStacks(stacks);

					for (auto stack : stacks)
					{
						if (!restrictItems.Contains(stack.Item.GetItemClass()))
						{
							continue;
						}

						out_injectedItems.Add(stack.Item.GetItemClass());
					}
				}
			}

			if (!configuration.ignoreStorageTeleporter &&
				!buildable && fullClassName.EndsWith(TEXT("/StorageTeleporter/Buildables/ItemTeleporter/ItemTeleporter_Build.ItemTeleporter_Build_C")))
			{
				buildable = storageTeleporter = Cast<AFGBuildableFactory>(owner);
			}

			if (buildable)
			{
				TArray<UFGFactoryConnectionComponent*> tempComponents;
				buildable->GetComponents(tempComponents);
				auto components = TSet<UFGFactoryConnectionComponent*>(tempComponents);

				if (cargoPlatform)
				{
					auto trackId = cargoPlatform->GetTrackGraphID();

					auto railroadSubsystem = AFGRailroadSubsystem::Get(owner->GetWorld());

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
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

								overflow = true;
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
								*indent,
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
									*indent,
									TEXT("    Station = "),
									*station->GetStationIdentifier()->GetStationName().ToString()
									);

								if (i == 0 && connectedPlatform->IsOrientationReversed() ||
									i == 1 && !connectedPlatform->IsOrientationReversed())
								{
									stationOffsets.insert(offsetDistance);
									EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), offsetDistance);
								}
								else
								{
									stationOffsets.insert(-offsetDistance);
									EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), -offsetDistance);
								}
							}

							if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
							{
								auto cargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
								if (cargo)
								{
									EC_LOG_Display(
										/**getTimeStamp(),*/
										*indent,
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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains!"));

							overflow = true;
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
									*indent,
									TEXT("Train = "),
									*train->GetTrainName().ToString()
									);
							}
							else
							{
								EC_LOG_Display(
									/**getTimeStamp(),*/
									*indent,
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
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout iterating train stops!"));

								overflow = true;
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
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout iterating train stops!"));

								overflow = true;
								return;
							}

							if (!stop.Station || !stop.Station->GetStation())
							{
								continue;
							}

							EC_LOG_Display_Condition(
								/**getTimeStamp(),*/
								*indent,
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
									if (timeout < time(NULL))
									{
										EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platformst!"));

										overflow = true;
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

									seenActors.Add(stopCargo);

									components.Append(
										stopCargo->GetConnectionComponents().FilterByPredicate(
											[&components, stopCargo](UFGFactoryConnectionComponent* connection)
											{
												return !components.Contains(connection) &&
													(stopCargo->GetIsInLoadMode() || connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT);
											}
											)
										);
								}
							}
						}
					}
				}

				if (storageTeleporter)
				{
					// Find all others of the same type
					auto currentStorageID = FReflectionHelper::GetPropertyValue<FStrProperty>(storageTeleporter, TEXT("StorageID"));

					// TArray<AActor*> allTeleporters;
					// if (IsInGameThread())
					// {
					//     UGameplayStatics::GetAllActorsOfClass(storageTeleporter->GetWorld(), storageTeleporter->GetClass(), allTeleporters);
					// }

					FScopeLock ScopeLock(&singleton->eclCritical);

					for (auto testTeleporter : AEfficiencyCheckerLogic::singleton->allTeleporters)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating storage teleporters!"));

							overflow = true;
							return;
						}

						if (IsValid(testTeleporter) || testTeleporter == storageTeleporter)
						{
							continue;
						}

						auto storageID = FReflectionHelper::GetPropertyValue<FStrProperty>(testTeleporter, TEXT("StorageID"));
						if (storageID == currentStorageID)
						{
							seenActors.Add(testTeleporter);

							auto factory = Cast<AFGBuildableFactory>(testTeleporter);
							if (factory)
							{
								components.Append(
									factory->GetConnectionComponents().FilterByPredicate(
										[&components](UFGFactoryConnectionComponent* connection)
										{
											return !components.Contains(connection); // Not in use already
										}
										)
									);
							}
						}
					}
				}

				if (undergroundBelt)
				{
					TSet<AActor*> undergroundActors;
					collectUndergroundBeltsComponents(undergroundBelt, components, undergroundActors);
					seenActors.Append(undergroundActors);
				}

				int currentOutputIndex = -1;
				std::map<int, TSet<TSubclassOf<UFGItemDescriptor>>> restrictedItemsByOutput;

				// Filter items
				auto smartSplitter = Cast<AFGBuildableSplitterSmart>(buildable);
				if (smartSplitter)
				{
					for (auto connection : components)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating smart splitters connectors!"));

							overflow = true;
							return;
						}

						if (connection->GetConnector() != EFactoryConnectionConnector::FCC_CONVEYOR)
						{
							continue;
						}

						if (connection->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT)
						{
							continue;
						}

						auto outputIndex = connection->GetName()[connection->GetName().Len() - 1] - '1';

						if (connection == connector)
						{
							currentOutputIndex = outputIndex;
						}
					}

					// Already restricted. Restrict further
					for (int x = 0; x < smartSplitter->GetNumSortRules(); ++x)
					{
						auto rule = smartSplitter->GetSortRuleAt(x);

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
							TEXT("Rule "),
							x,
							TEXT(" / output index = "),
							rule.OutputIndex,
							TEXT(" / item = "),
							*UFGItemDescriptor::GetItemName(rule.ItemClass).ToString(),
							TEXT(" / class = "),
							*GetPathNameSafe(rule.ItemClass)
							);

						restrictedItemsByOutput[rule.OutputIndex].Add(rule.ItemClass);
					}

					TSet<TSubclassOf<UFGItemDescriptor>> definedItems;

					// First pass
					for (auto it = restrictedItemsByOutput.begin(); it != restrictedItemsByOutput.end(); ++it)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating restricted items!"));

							overflow = true;
							return;
						}

						if (singleton->noneItemDescriptors.Intersect(it->second).Num())
						{
							// No item is valid. Empty it all
							it->second.Empty();
						}
						else if (singleton->wildCardItemDescriptors.Intersect(it->second).Num() || singleton->overflowItemDescriptors.Intersect(it->second).Num())
						{
							// Add all current restrictItems as valid items
							it->second = restrictItems;
						}

						for (auto item : it->second)
						{
							definedItems.Add(item);
						}
					}

					for (auto it = restrictedItemsByOutput.begin(); it != restrictedItemsByOutput.end(); ++it)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating restricted items!"));

							overflow = true;
							return;
						}

						if (singleton->anyUndefinedItemDescriptors.Intersect(it->second).Num())
						{
							it->second = it->second.Union(restrictItems.Difference(definedItems));
						}

						if (it->first == currentOutputIndex && !it->second.Num())
						{
							// Can't go further. Return
							return;
						}
					}
				}

				TArray<UFGFactoryConnectionComponent*> connectedInputs, connectedOutputs;
				for (auto connection : components)
				{
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

						overflow = true;
						return;
					}

					if (!connection->IsConnected())
					{
						continue;
					}

					if (connection->GetConnector() != EFactoryConnectionConnector::FCC_CONVEYOR)
					{
						continue;
					}

					if (connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
					{
						connectedInputs.Add(connection);
					}
					else if (connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
					{
						connectedOutputs.Add(connection);
					}
				}

				if (connectedInputs.Num() == 1 && connectedOutputs.Num() == 1)
				{
					connected.Add(buildable);

					connector = connectedInputs[0]->GetConnection();

					EC_LOG_Display_Condition(*indent, *buildable->GetName(), TEXT(" skipped"));

					if (smartSplitter)
					{
						auto outputIndex = connectedOutputs[0]->GetName()[connectedOutputs[0]->GetName().Len() - 1] - '1';

						restrictItems = restrictedItemsByOutput[outputIndex];
					}

					continue;
				}

				if (connectedInputs.Num() == 0)
				{
					// Nothing is being inputed. Bail
					EC_LOG_Error_Condition(*indent, *buildable->GetName(), TEXT(" has no input"));
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					for (auto connection : components)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							overflow = true;
							return;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						if (connection->GetConnector() != EFactoryConnectionConnector::FCC_CONVEYOR)
						{
							continue;
						}

						if (connection->GetDirection() != EFactoryConnectionDirection::FCD_INPUT)
						{
							continue;
						}

						if (dockingStation && connection->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase))
						{
							continue;
						}

						float previousLimit = out_limitedThroughput;
						collectInput(
							resourceForm,
							customInjectedInput,
							connection->GetConnection(),
							out_injectedInput,
							previousLimit,
							seenActors,
							connected,
							out_injectedItems,
							currentOutputIndex < 0 ? restrictItems : restrictedItemsByOutput[currentOutputIndex],
							buildableSubsystem,
							level + 1,
							overflow,
							indent + TEXT("    "),
							timeout,
							machineStatusIncludeType
							);

						if (overflow)
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

					out_limitedThroughput = FMath::Min(out_limitedThroughput, limitedThroughput);

					for (auto connection : components)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							overflow = true;
							return;
						}

						if (connection == connector)
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						if (connection->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT)
						{
							continue;
						}

						auto outputIndex = connection->GetName()[connection->GetName().Len() - 1] - '1';

						float previousLimit = 0;
						float discountedInput = 0;
						std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>> seenActorsCopy;

						auto tempInjectedItems = out_injectedItems;

						if (currentOutputIndex >= 0)
						{
							tempInjectedItems = tempInjectedItems.Intersect(restrictedItemsByOutput[outputIndex]);
						}

						for (auto actor : seenActors)
						{
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating seen actors!"));

								overflow = true;
								return;
							}

							seenActorsCopy[actor] = out_injectedItems;
						}

						collectOutput(
							resourceForm,
							connection->GetConnection(),
							discountedInput,
							previousLimit,
							seenActorsCopy,
							connected,
							tempInjectedItems,
							buildableSubsystem,
							level + 1,
							overflow,
							indent + TEXT("    "),
							timeout,
							machineStatusIncludeType
							);

						if (overflow)
						{
							return;
						}

						if (discountedInput > 0)
						{
							EC_LOG_Display_Condition(*indent, TEXT("Discounting "), discountedInput, TEXT(" items/minute"));

							if (!customInjectedInput)
							{
								out_injectedInput -= discountedInput;
							}
						}
					}

					if (dockingStation /*|| cargoPlatform*/)
					{
						out_injectedInput += out_limitedThroughput;
					}

					EC_LOG_Display_Condition(
						/**getTimeStamp(),*/
						*indent,
						*buildable->GetName(),
						TEXT(" limited at "),
						out_limitedThroughput,
						TEXT(" items/minute")
						);
				}

				connected.Add(buildable);

				return;
			}
		}

		if (resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS)
		{
			auto pipeline = Cast<AFGBuildablePipeline>(owner);
			// if (pipeline)
			// {
			//     // out_limitedThroughput = UFGBlueprintFunctionLibrary::RoundFloatWithPrecision(pipeline->GetFlowLimit() * 60, 4);
			//     // // out_limitedThroughput = pipeline->mFlowLimit * 60;
			//     //
			//     // //out_limitedThroughput = 300;
			//     //
			//     // auto components = pipeline->GetPipeConnections();
			//     //
			//     // for (auto connection : components)
			//     // {
			//     //     if (!connection->IsConnected() ||
			//     //         connection->GetPipeConnection()->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER /*||
			//     //     connection == connector*/)
			//     //     {
			//     //         continue;
			//     //     }
			//     //
			//     //     float previousLimit = out_limitedThroughput;
			//     //     collectInput(
			//     //         resourceForm,
			//     //         customInjectedInput,
			//     //         connection->GetConnection(),
			//     //         out_injectedInput,
			//     //         previousLimit,
			//     //         seenActors,
			//     //         connected,
			//     //         out_injectedItems,
			//     //         restrictItems,
			//     //         buildableSubsystem,
			//     //         level + 1,
			//     //         indent + TEXT("    ")
			//     //         );
			//     //
			//     //     out_limitedThroughput = FMath::Min(out_limitedThroughput, previousLimit);
			//     // }
			//
			//     auto flowLimit = getPipeSpeed(pipeline);
			//
			//     out_limitedThroughput = FMath::Min(out_limitedThroughput, flowLimit);
			//
			//     if (configuration.dumpConnections)
			//     {
			//         EC_LOG_Display(*indent, *pipeline->GetName(), TEXT(" flow limit = "), flowLimit, TEXT(" m³/minute"));
			//         EC_LOG_Display(*indent, *pipeline->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" m³/minute"));
			//     }
			//
			//     connected.Add(pipeline);
			//
			//     // Get the opposing connector
			//     connector = connector == pipeline->GetPipeConnection0()
			//                     ? pipeline->GetPipeConnection1()->GetConnection()
			//                     : pipeline->GetPipeConnection0()->GetConnection();
			//
			//     continue;
			// }

			auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(owner);
			if (fluidIntegrant)
			{
				auto buildable = Cast<AFGBuildable>(owner);

				auto components = fluidIntegrant->GetPipeConnections();

				if (pipeline)
				{
					out_limitedThroughput = FMath::Min(out_limitedThroughput, getPipeSpeed(pipeline));
				}

				auto otherConnections = seenActors.Num() == 1
					                        ? components
					                        : components.FilterByPredicate(
						                        [connector, seenActors](UFGPipeConnectionComponent* pipeConnection)
						                        {
							                        return
								                        pipeConnection != connector && pipeConnection->IsConnected();
						                        }
						                        );

				auto pipePump = Cast<AFGBuildablePipelinePump>(fluidIntegrant);
				auto pipeConnection = Cast<UFGPipeConnectionComponent>(connector);

				if (pipePump && pipePump->GetUserFlowLimit() > 0 && components.Num() == 2 && components[0]->IsConnected() && components[1]->IsConnected())
				{
					auto pipe0 = Cast<AFGBuildablePipeline>(components[0]->GetPipeConnection()->GetOwner());
					auto pipe1 = Cast<AFGBuildablePipeline>(components[1]->GetPipeConnection()->GetOwner());

					out_limitedThroughput = FMath::Min(
						out_limitedThroughput,
						UFGBlueprintFunctionLibrary::RoundFloatWithPrecision(
							FMath::Min(getPipeSpeed(pipe0), getPipeSpeed(pipe1)) * pipePump->GetUserFlowLimit() / pipePump->GetDefaultFlowLimit(),
							4
							)
						);
				}

				if (otherConnections.Num() == 0)
				{
					// No more connections. Bail
					EC_LOG_Error_Condition(*indent, *owner->GetName(), TEXT(" has no other connection"));
				}
				else if (otherConnections.Num() == 1 &&
					(otherConnections[0]->GetPipeConnectionType() != EPipeConnectionType::PCT_CONSUMER &&
						otherConnections[0]->GetPipeConnectionType() != EPipeConnectionType::PCT_PRODUCER &&
						GetConnectedPipeConnectionType(otherConnections[0]) == EPipeConnectionType::PCT_ANY ||
						pipeConnection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER &&
						otherConnections[0]->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER))
				{
					connected.Add(buildable);

					connector = otherConnections[0]->GetConnection();

					EC_LOG_Display_Condition(*indent, *buildable->GetName(), TEXT(" skipped"));

					continue;
				}
				else
				{
					bool firstConnection = true;
					float limitedThroughput = 0;

					bool firstActor = seenActors.Num() == 1;

					for (auto connection : components)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							overflow = true;
							return;
						}

						if (connection == connector && !firstActor)
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						if (connection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER ||
							GetConnectedPipeConnectionType(connection) == EPipeConnectionType::PCT_CONSUMER)
						{
							continue;
						}

						float previousLimit = out_limitedThroughput;
						collectInput(
							resourceForm,
							customInjectedInput,
							connection->GetConnection(),
							out_injectedInput,
							previousLimit,
							seenActors,
							connected,
							out_injectedItems,
							restrictItems,
							buildableSubsystem,
							level + 1,
							overflow,
							indent + TEXT("    "),
							timeout,
							machineStatusIncludeType
							);

						if (overflow)
						{
							return;
						}

						if (pipeline)
						{
							out_limitedThroughput = FMath::Min(out_limitedThroughput, previousLimit);
						}
						else if (firstConnection)
						{
							limitedThroughput = previousLimit;
							firstConnection = false;
						}
						else
						{
							limitedThroughput += limitedThroughput;
						}
					}

					if (!pipeline && !firstConnection)
					{
						out_limitedThroughput = FMath::Min(out_limitedThroughput, limitedThroughput);
					}

					auto pipeConnector = Cast<UFGPipeConnectionComponent>(connector);

					if (pipePump && pipeConnector->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER)
					{
						for (auto connection : components)
						{
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

								overflow = true;
								return;
							}

							if (connection == connector)
							{
								continue;
							}

							if (!connection->IsConnected() || connection->GetPipeConnectionType() != EPipeConnectionType::PCT_CONSUMER)
							{
								continue;
							}

							float previousLimit = 0;
							float discountedInput = 0;

							std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>> seenActorsCopy;

							for (auto actor : seenActors)
							{
								if (timeout < time(NULL))
								{
									EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating seen actors!"));

									overflow = true;
									return;
								}

								seenActorsCopy[actor] = out_injectedItems;
							}

							collectOutput(
								resourceForm,
								connection->GetConnection(),
								discountedInput,
								previousLimit,
								seenActorsCopy,
								connected,
								out_injectedItems,
								buildableSubsystem,
								level + 1,
								overflow,
								indent + TEXT("    "),
								timeout,
								machineStatusIncludeType
								);

							if (overflow)
							{
								return;
							}

							if (discountedInput > 0)
							{
								EC_LOG_Display_Condition(*indent, TEXT("Discounting "), discountedInput, TEXT(" m³/minute"));

								if (!customInjectedInput)
								{
									out_injectedInput -= discountedInput;
								}
							}
						}
					}

					EC_LOG_Display_Condition(*indent, *owner->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" m³/minute"));
				}

				connected.Add(buildable);

				return;
			}

			auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner);
			if (cargoPlatform)
			{
				TArray<UFGPipeConnectionComponent*> pipeConnections;
				cargoPlatform->GetComponents(pipeConnections);

				auto trackId = cargoPlatform->GetTrackGraphID();

				auto railroadSubsystem = AFGRailroadSubsystem::Get(owner->GetWorld());

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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

							overflow = true;
							return;
						}

						if (seenPlatforms.Contains(connectedPlatform))
						{
							// Loop detected
							break;
						}

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
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
								*indent,
								TEXT("    Station = "),
								*station->GetStationIdentifier()->GetStationName().ToString()
								);

							if (i == 0 && connectedPlatform->IsOrientationReversed() ||
								i == 1 && !connectedPlatform->IsOrientationReversed())
							{
								stationOffsets.insert(offsetDistance);
								EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), offsetDistance);
							}
							else
							{
								stationOffsets.insert(-offsetDistance);
								EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), -offsetDistance);
							}
						}

						if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
						{
							auto cargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
							if (cargo)
							{
								EC_LOG_Display(
									/**getTimeStamp(),*/
									*indent,
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
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains!"));

						overflow = true;
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
								*indent,
								TEXT("Train = "),
								*train->GetTrainName().ToString()
								);
						}
						else
						{
							EC_LOG_Display(
								/**getTimeStamp(),*/
								*indent,
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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

							overflow = true;
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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

							overflow = true;
							return;
						}

						if (!stop.Station || !stop.Station->GetStation())
						{
							continue;
						}

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
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
								if (timeout < time(NULL))
								{
									EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

									overflow = true;
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

								seenActors.Add(stopCargo);

								TArray<UFGPipeConnectionComponent*> cargoPipeConnections;
								stopCargo->GetComponents(cargoPipeConnections);

								pipeConnections.Append(
									cargoPipeConnections.FilterByPredicate(
										[&pipeConnections, stopCargo](UFGPipeConnectionComponent* connection)
										{
											if (pipeConnections.Contains(connection))
											{
												// Already in use
												return false;
											}

											if (stopCargo->GetIsInLoadMode())
											{
												// Loading
												return true;
											}

											if (connection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER)
											{
												// Is not a consumer connection
												return true;
											}

											return false;
										}
										)
									);
							}
						}
					}
				}

				bool firstConnection = true;
				float limitedThroughput = out_limitedThroughput;

				for (auto connection : pipeConnections)
				{
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating pipe connectors!"));

						overflow = true;
						return;
					}

					if (!connection->IsConnected() ||
						connection->GetPipeConnectionType() != EPipeConnectionType::PCT_CONSUMER ||
						connection == connector)
					{
						continue;
					}

					float previousLimit = out_limitedThroughput;
					collectInput(
						resourceForm,
						customInjectedInput,
						connection->GetConnection(),
						out_injectedInput,
						previousLimit,
						seenActors,
						connected,
						out_injectedItems,
						restrictItems,
						buildableSubsystem,
						level + 1,
						overflow,
						indent + TEXT("    "),
						timeout,
						machineStatusIncludeType
						);

					if (overflow)
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

				out_limitedThroughput = FMath::Min(out_limitedThroughput, limitedThroughput);

				for (auto connection : pipeConnections)
				{
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating pipe connectors!"));

						overflow = true;
						return;
					}

					if (connection == connector ||
						!connection->IsConnected() ||
						connection->GetPipeConnectionType() != EPipeConnectionType::PCT_PRODUCER)
					{
						continue;
					}

					float previousLimit = 0;
					float discountedInput = 0;

					std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>> seenActorsCopy;

					for (auto actor : seenActors)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating seen actors!"));

							overflow = true;
							return;
						}

						seenActorsCopy[actor] = out_injectedItems;
					}

					collectOutput(
						resourceForm,
						connection->GetConnection(),
						discountedInput,
						previousLimit,
						seenActorsCopy,
						connected,
						out_injectedItems,
						buildableSubsystem,
						level + 1,
						overflow,
						indent + TEXT("    "),
						timeout,
						machineStatusIncludeType
						);

					if (overflow)
					{
						return;
					}

					if (discountedInput > 0)
					{
						EC_LOG_Display_Condition(*indent, TEXT("Discounting "), discountedInput, TEXT(" m³/minute"));

						if (!customInjectedInput)
						{
							out_injectedInput -= discountedInput;
						}
					}
				}

				EC_LOG_Display_Condition(*indent, *owner->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" m³/minute"));

				connected.Add(Cast<AFGBuildable>(fluidIntegrant));

				return;
			}
		}

		{
			const auto nuclearGenerator = Cast<AFGBuildableGeneratorNuclear>(owner);
			if (nuclearGenerator &&
				(nuclearGenerator->HasPower() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!nuclearGenerator->IsProductionPaused() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Paused)))
			{
				for (auto item : singleton->nuclearWasteItemDescriptors)
				{
					out_injectedItems.Add(item);
				}

				connected.Add(nuclearGenerator);

				out_injectedInput += 0.2;

				return;
			}
		}

		if (inheritsFrom(owner, TEXT("/Script/FactoryGame.FGBuildableFactorySimpleProducer")))
		{
			TSubclassOf<UFGItemDescriptor> itemType = FReflectionHelper::GetObjectPropertyValue<UClass>(owner, TEXT("mItemType"));
			auto timeToProduceItem = FReflectionHelper::GetPropertyValue<FFloatProperty>(owner, TEXT("mTimeToProduceItem"));

			if (timeToProduceItem && itemType)
			{
				out_injectedItems.Add(itemType);

				out_injectedInput += 60 / timeToProduceItem;
			}

			connected.Add(Cast<AFGBuildable>(owner));

			return;
		}

		// out_limitedThroughput = 0;

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			dumpUnknownClass(indent, owner);
		}

		return;
	}
}

void AEfficiencyCheckerLogic::collectOutput
(
	EResourceForm resourceForm,
	class UFGConnectionComponent* connector,
	float& out_requiredOutput,
	float& out_limitedThroughput,
	std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& seenActors,
	TSet<AFGBuildable*>& connected,
	const TSet<TSubclassOf<UFGItemDescriptor>>& in_injectedItems,
	class AFGBuildableSubsystem* buildableSubsystem,
	int level,
	bool& overflow,
	const FString& indent,
	const time_t& timeout,
	int32 machineStatusIncludeType
)
{
	TSet<TSubclassOf<UFGItemDescriptor>> injectedItems = in_injectedItems;

	for (;;)
	{
		if (!connector)
		{
			return;
		}

		auto owner = connector->GetOwner();

		if (!owner)
		{
			return;
		}

		if (timeout < time(NULL))
		{
			EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout!"));

			overflow = true;
			return;
		}

		auto fullClassName = GetPathNameSafe(owner->GetClass());

		if (level > 100)
		{
			EC_LOG_Error_Condition(
				FUNCTIONSTR TEXT(": level is too deep: "),
				level,
				TEXT("; "),
				*owner->GetName(),
				TEXT(" / "),
				*fullClassName
				);

			overflow = true;

			return;
		}

		if (injectedItems.Num())
		{
			bool unusedItems = false;

			for (auto item : injectedItems)
			{
				if (!actorContainsItem(seenActors, owner, item))
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
			if (containsActor(seenActors, owner))
			{
				return;
			}
		}

		EC_LOG_Display_Condition(
			/**getTimeStamp(),*/
			*indent,
			TEXT("collectOutput at level "),
			level,
			TEXT(": "),
			*owner->GetName(),
			TEXT(" / "),
			*fullClassName
			);

		{
			const auto manufacturer = Cast<AFGBuildableManufacturer>(owner);
			if (manufacturer &&
				(manufacturer->HasPower() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!manufacturer->IsProductionPaused() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Paused)))
			{
				const auto recipeClass = manufacturer->GetCurrentRecipe();

				if (recipeClass)
				{
					auto ingredients = UFGRecipe::GetIngredients(recipeClass);

					for (auto item : ingredients)
					{
						auto itemForm = UFGItemDescriptor::GetForm(item.ItemClass);

						if (itemForm == EResourceForm::RF_SOLID && resourceForm != EResourceForm::RF_SOLID ||
							(itemForm == EResourceForm::RF_LIQUID || itemForm == EResourceForm::RF_GAS) &&
							resourceForm != EResourceForm::RF_LIQUID && resourceForm != EResourceForm::RF_GAS)
						{
							continue;
						}

						if (!injectedItems.Contains(item.ItemClass) || seenActors[manufacturer].Contains(item.ItemClass))
						{
							continue;
						}

						if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
						{
							EC_LOG_Display(*indent, TEXT("Item amount = "), item.Amount);
							EC_LOG_Display(*indent, TEXT("Current potential = "), manufacturer->GetCurrentPotential());
							EC_LOG_Display(*indent, TEXT("Pending potential = "), manufacturer->GetPendingPotential());
							EC_LOG_Display(
								*indent,
								TEXT("Production cycle time = "),
								manufacturer->CalcProductionCycleTimeForPotential(manufacturer->GetPendingPotential())
								);
							EC_LOG_Display(*indent, TEXT("Recipe duration = "), UFGRecipe::GetManufacturingDuration(recipeClass));
						}

						float itemAmountPerMinute = item.Amount * manufacturer->GetPendingPotential() * 60
							/ manufacturer->GetDefaultProductionCycleTime();

						if (resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS)
						{
							itemAmountPerMinute /= 1000;
						}

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
							*manufacturer->GetName(),
							TEXT(" consumes "),
							itemAmountPerMinute,
							TEXT(" "),
							*UFGItemDescriptor::GetItemName(item.ItemClass).ToString(),
							TEXT("/minute")
							);

						out_requiredOutput += itemAmountPerMinute;

						seenActors[manufacturer].Add(item.ItemClass);
					}
				}

				connected.Add(manufacturer);

				return;
			}
		}

		if (resourceForm == EResourceForm::RF_SOLID)
		{
			const auto conveyor = Cast<AFGBuildableConveyorBase>(owner);
			if (conveyor)
			{
				addAllItemsToActor(seenActors, conveyor, injectedItems);

				// // The innitial limit for a belt is its own speed
				// out_limitedThroughput = conveyor->GetSpeed() / 2;
				//
				// const auto conveyorInput = conveyor->GetConnection1();
				// if (conveyorInput && conveyorInput->IsConnected())
				// {
				//     float previousLimit = out_limitedThroughput;
				//     collectOutput(
				//         resourceForm,
				//         conveyorInput->GetConnection(),
				//         out_requiredOutput,
				//         previousLimit,
				//         seenActors,
				//         connected,
				//         injectedItems,
				//         buildableSubsystem,
				//         level + 1,
				//         indent + TEXT("    ")
				//         );
				//
				//     out_limitedThroughput = FMath::Min(out_limitedThroughput, previousLimit);
				// }

				connected.Add(conveyor);

				connector = conveyor->GetConnection1()->GetConnection();

				out_limitedThroughput = FMath::Min(out_limitedThroughput, conveyor->GetSpeed() / 2);

				EC_LOG_Display_Condition(*indent, *conveyor->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" items/minute"));

				continue;
			}

			AFGBuildableStorage* storageContainer = nullptr;
			AFGBuildableTrainPlatformCargo* cargoPlatform = nullptr;
			AFGBuildableConveyorAttachment* conveyorAttachment = nullptr;
			AFGBuildableDockingStation* dockingStation = nullptr;
			AFGBuildableFactory* storageTeleporter = nullptr;
			AFGBuildableStorage* undergroundBelt = nullptr;

			AFGBuildable* buildable = Cast<AFGBuildableConveyorAttachment>(owner);

			if (!buildable && (fullClassName.EndsWith("/UndergroundBelts/Build/Build_UndergroundSplitterInput.Build_UndergroundSplitterInput_C") ||
				fullClassName.EndsWith("/UndergroundBelts/Build/Build_UndergroundSplitterOutput.Build_UndergroundSplitterOutput_C")))
			{
				buildable = undergroundBelt = Cast<AFGBuildableStorage>(owner);
			}

			if (!buildable)
			{
				buildable = storageContainer = Cast<AFGBuildableStorage>(owner);
			}

			if (!buildable)
			{
				buildable = cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner);
			}

			if (!buildable)
			{
				buildable = dockingStation = Cast<AFGBuildableDockingStation>(owner);
			}

			if (!configuration.ignoreStorageTeleporter &&
				!buildable && fullClassName.EndsWith(TEXT("/StorageTeleporter/Buildables/ItemTeleporter/ItemTeleporter_Build.ItemTeleporter_Build_C")))
			{
				buildable = storageTeleporter = Cast<AFGBuildableFactory>(owner);
			}

			if (buildable)
			{
				addAllItemsToActor(seenActors, buildable, injectedItems);

				TArray<UFGFactoryConnectionComponent*> tempComponents;
				buildable->GetComponents(tempComponents);
				auto components = TSet<UFGFactoryConnectionComponent*>(tempComponents);

				if (cargoPlatform)
				{
					auto trackId = cargoPlatform->GetTrackGraphID();

					auto railroadSubsystem = AFGRailroadSubsystem::Get(owner->GetWorld());

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
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

								overflow = true;
								return;
							}

							if (seenPlatforms.Contains(connectedPlatform))
							{
								// Loop detected
								break;
							}

							EC_LOG_Display_Condition(
								/**getTimeStamp(),*/
								*indent,
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
									*indent,
									TEXT("    Station = "),
									*station->GetStationIdentifier()->GetStationName().ToString()
									);

								if (i == 0 && connectedPlatform->IsOrientationReversed() ||
									i == 1 && !connectedPlatform->IsOrientationReversed())
								{
									stationOffsets.insert(offsetDistance);
									EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), offsetDistance);
								}
								else
								{
									stationOffsets.insert(-offsetDistance);
									EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), -offsetDistance);
								}
							}

							auto cargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
							if (cargo)
							{
								EC_LOG_Display_Condition(
									/**getTimeStamp(),*/
									*indent,
									TEXT("    Load mode = "),
									cargo->GetIsInLoadMode() ? TEXT("true") : TEXT("false")
									);
							}
						}
					}

					TArray<AFGTrain*> trains;
					railroadSubsystem->GetTrains(trackId, trains);

					for (auto train : trains)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains!"));

							overflow = true;
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
									*indent,
									TEXT("Train = "),
									*train->GetTrainName().ToString()
									);
							}
							else
							{
								EC_LOG_Display(
									/**getTimeStamp(),*/
									*indent,
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
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains stops!"));

								overflow = true;
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
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

								overflow = true;
								return;
							}

							if (!stop.Station || !stop.Station->GetStation())
							{
								continue;
							}

							EC_LOG_Display_Condition(
								/**getTimeStamp(),*/
								*indent,
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
									if (timeout < time(NULL))
									{
										EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

										overflow = true;
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

									auto adjustedOffsetDistance = i == 0 && !stop.Station->GetStation()->IsOrientationReversed()
									                              || i == 1 && stop.Station->GetStation()->IsOrientationReversed()
										                              ? offsetDistance
										                              : -offsetDistance;

									if (stationOffsets.find(adjustedOffsetDistance) == stationOffsets.end())
									{
										// Not on a valid offset. Skip
										continue;
									}

									addAllItemsToActor(seenActors, stopCargo, injectedItems);

									components.Append(
										stopCargo->GetConnectionComponents().FilterByPredicate(
											[&components, stopCargo](UFGFactoryConnectionComponent* connection)
											{
												return !components.Contains(connection) && // Not in use already
													!stopCargo->GetIsInLoadMode() && // Unload mode
													connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT; // Is output connection
											}
											)
										);
								}
							}
						}
					}
				}

				if (storageTeleporter)
				{
					auto currentStorageID = FReflectionHelper::GetPropertyValue<FStrProperty>(storageTeleporter, TEXT("StorageID"));

					// TArray<AActor*> allTeleporters;
					// if (IsInGameThread())
					// {
					//     UGameplayStatics::GetAllActorsOfClass(storageTeleporter->GetWorld(), storageTeleporter->GetClass(), allTeleporters);
					// }

					FScopeLock ScopeLock(&singleton->eclCritical);

					for (auto testTeleporter : AEfficiencyCheckerLogic::singleton->allTeleporters)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating teleporters!"));

							overflow = true;
							return;
						}

						if (IsValid(testTeleporter) || testTeleporter == storageTeleporter)
						{
							continue;
						}

						auto storageID = FReflectionHelper::GetPropertyValue<FStrProperty>(testTeleporter, TEXT("StorageID"));
						if (storageID == currentStorageID)
						{
							addAllItemsToActor(seenActors, testTeleporter, injectedItems);

							auto factory = Cast<AFGBuildableFactory>(testTeleporter);
							if (factory)
							{
								components.Append(
									factory->GetConnectionComponents().FilterByPredicate(
										[&components](UFGFactoryConnectionComponent* connection)
										{
											return !components.Contains(connection) && // Not in use already
												connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT; // Is output connection
										}
										)
									);
							}
						}
					}
				}

				if (undergroundBelt)
				{
					TSet<AActor*> undergroundActors;
					collectUndergroundBeltsComponents(undergroundBelt, components, undergroundActors);
					for (auto undergroundActor : undergroundActors)
					{
						addAllItemsToActor(seenActors, undergroundActor, injectedItems);
					}
				}

				std::map<int, TSet<TSubclassOf<UFGItemDescriptor>>> restrictedItemsByOutput;

				// Filter items
				auto smartSplitter = Cast<AFGBuildableSplitterSmart>(buildable);
				if (smartSplitter)
				{
					for (int x = 0; x < smartSplitter->GetNumSortRules(); ++x)
					{
						auto rule = smartSplitter->GetSortRuleAt(x);

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
							TEXT("Rule "),
							x,
							TEXT(" / output index = "),
							rule.OutputIndex,
							TEXT(" / item = "),
							*UFGItemDescriptor::GetItemName(rule.ItemClass).ToString(),
							TEXT(" / class = "),
							*GetPathNameSafe(rule.ItemClass)
							);

						restrictedItemsByOutput[rule.OutputIndex].Add(rule.ItemClass);
					}

					TSet<TSubclassOf<UFGItemDescriptor>> definedItems;

					for (auto it = restrictedItemsByOutput.begin(); it != restrictedItemsByOutput.end(); ++it)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating restricted items!"));

							overflow = true;
							return;
						}

						if (singleton->noneItemDescriptors.Intersect(it->second).Num())
						{
							// No item is valid. Empty it all
							it->second.Empty();
						}
						else if (singleton->wildCardItemDescriptors.Intersect(it->second).Num() || singleton->overflowItemDescriptors.Intersect(it->second).Num())
						{
							// Add all current restrictItems as valid items
							it->second = injectedItems;
						}

						for (auto item : it->second)
						{
							definedItems.Add(item);
						}
					}

					for (auto it = restrictedItemsByOutput.begin(); it != restrictedItemsByOutput.end(); ++it)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating restricted items!"));

							overflow = true;
							return;
						}

						if (singleton->anyUndefinedItemDescriptors.Intersect(it->second).Num())
						{
							it->second = it->second.Union(injectedItems.Difference(definedItems));
						}

						it->second = it->second.Intersect(injectedItems);
					}
				}


				TArray<UFGFactoryConnectionComponent*> connectedInputs, connectedOutputs;
				for (auto connection : components)
				{
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

						overflow = true;
						return;
					}

					if (!connection->IsConnected())
					{
						continue;
					}

					if (connection->GetConnector() != EFactoryConnectionConnector::FCC_CONVEYOR)
					{
						continue;
					}

					if (connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
					{
						connectedInputs.Add(connection);
					}
					else if (connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
					{
						connectedOutputs.Add(connection);
					}
				}

				if (connectedInputs.Num() == 1 && connectedOutputs.Num() == 1)
				{
					connected.Add(buildable);

					connector = connectedOutputs[0]->GetConnection();

					EC_LOG_Display_Condition(*indent, *buildable->GetName(), TEXT(" skipped"));

					if (smartSplitter)
					{
						auto outputIndex = connectedOutputs[0]->GetName()[connectedOutputs[0]->GetName().Len() - 1] - '1';

						injectedItems = restrictedItemsByOutput[outputIndex];
					}

					continue;
				}

				if (connectedOutputs.Num() == 0)
				{
					// Nothing is being outputed. Bail
					EC_LOG_Error_Condition(*indent, *buildable->GetName(), TEXT(" has no input"));
				}
				else
				{
					if (!dockingStation || !connector->GetName().Equals(TEXT("Input0"), ESearchCase::IgnoreCase))
					{
						bool firstConnection = true;
						float limitedThroughput = 0;

						for (auto connection : components)
						{
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

								overflow = true;
								return;
							}

							if (!connection->IsConnected())
							{
								continue;
							}

							if (connection->GetConnector() != EFactoryConnectionConnector::FCC_CONVEYOR)
							{
								continue;
							}

							if (connection->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT)
							{
								continue;
							}

							auto outputIndex = connection->GetName()[connection->GetName().Len() - 1] - '1';

							float previousLimit = out_limitedThroughput;
							collectOutput(
								resourceForm,
								connection->GetConnection(),
								out_requiredOutput,
								previousLimit,
								seenActors,
								connected,
								smartSplitter ? restrictedItemsByOutput[outputIndex] : injectedItems,
								buildableSubsystem,
								level + 1,
								overflow,
								indent + TEXT("    "),
								timeout,
								machineStatusIncludeType
								);

							if (overflow)
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

						out_limitedThroughput = FMath::Min(out_limitedThroughput, limitedThroughput);

						for (auto connection : components)
						{
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

								overflow = true;
								return;
							}

							if (connection == connector)
							{
								continue;
							}

							if (!connection->IsConnected())
							{
								continue;
							}

							if (connection->GetDirection() != EFactoryConnectionDirection::FCD_INPUT)
							{
								continue;
							}

							auto inputIndex = connection->GetName()[connection->GetName().Len() - 1] - '1';

							float previousLimit = 0;
							float discountedOutput = 0;
							TSet<AActor*> seenActorsCopy;

							auto tempInjectedItems = injectedItems;

							for (auto actor : seenActors)
							{
								if (timeout < time(NULL))
								{
									EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating seen actors!"));

									overflow = true;
									return;
								}

								seenActorsCopy.Add(actor.first);
							}

							collectInput(
								resourceForm,
								false,
								connection->GetConnection(),
								discountedOutput,
								previousLimit,
								seenActorsCopy,
								connected,
								tempInjectedItems,
								tempInjectedItems,
								buildableSubsystem,
								level + 1,
								overflow,
								indent + TEXT("    "),
								timeout,
								machineStatusIncludeType
								);

							if (overflow)
							{
								return;
							}

							if (discountedOutput > 0)
							{
								EC_LOG_Display_Condition(*indent, TEXT("Discounting "), discountedOutput, TEXT(" items/minute"));

								out_requiredOutput -= discountedOutput;
							}
						}
					}

					EC_LOG_Display_Condition(
						/**getTimeStamp(),*/
						*indent,
						*buildable->GetName(),
						TEXT(" limited at "),
						out_limitedThroughput,
						TEXT(" items/minute")
						);
				}

				connected.Add(buildable);

				return;
			}
		}

		if (resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS)
		{
			auto pipeline = Cast<AFGBuildablePipeline>(owner);

			auto fluidIntegrant = Cast<IFGFluidIntegrantInterface>(owner);
			if (fluidIntegrant)
			{
				auto buildable = Cast<AFGBuildable>(fluidIntegrant);

				addAllItemsToActor(seenActors, Cast<AFGBuildable>(fluidIntegrant), injectedItems);

				auto components = fluidIntegrant->GetPipeConnections();

				if (pipeline)
				{
					out_limitedThroughput = FMath::Min(out_limitedThroughput, getPipeSpeed(pipeline));
				}

				auto otherConnections =
					seenActors.size() == 1
						? components
						: components.FilterByPredicate(
							[connector, seenActors](UFGPipeConnectionComponent* pipeConnection)
							{
								return pipeConnection != connector && pipeConnection->IsConnected();
							}
							);

				auto pipePump = Cast<AFGBuildablePipelinePump>(fluidIntegrant);
				auto pipeConnection = Cast<UFGPipeConnectionComponent>(connector);

				if (pipePump && pipePump->GetUserFlowLimit() > 0 && components.Num() == 2 && components[0]->IsConnected() && components[1]->IsConnected())
				{
					auto pipe0 = Cast<AFGBuildablePipeline>(components[0]->GetPipeConnection()->GetOwner());
					auto pipe1 = Cast<AFGBuildablePipeline>(components[1]->GetPipeConnection()->GetOwner());

					out_limitedThroughput = FMath::Min(
						out_limitedThroughput,
						UFGBlueprintFunctionLibrary::RoundFloatWithPrecision(
							FMath::Min(getPipeSpeed(pipe0), getPipeSpeed(pipe1)) * pipePump->GetUserFlowLimit() / pipePump->GetDefaultFlowLimit(),
							4
							)
						);
				}

				if (otherConnections.Num() == 0)
				{
					// No more connections. Bail
					EC_LOG_Error_Condition(*indent, *owner->GetName(), TEXT(" has no other connection"));
				}
				else if (otherConnections.Num() == 1 &&
					(otherConnections[0]->GetPipeConnectionType() != EPipeConnectionType::PCT_CONSUMER &&
						otherConnections[0]->GetPipeConnectionType() != EPipeConnectionType::PCT_PRODUCER &&
						GetConnectedPipeConnectionType(otherConnections[0]) == EPipeConnectionType::PCT_ANY ||
						pipeConnection->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER &&
						otherConnections[0]->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER))
				{
					connected.Add(buildable);

					connector = otherConnections[0]->GetConnection();

					EC_LOG_Display_Condition(*indent, *buildable->GetName(), TEXT(" skipped"));

					continue;
				}
				else
				{
					// out_limitedThroughput = 0;
					bool firstConnection = true;
					float limitedThroughput = out_limitedThroughput;

					bool firstActor = seenActors.size() == 1;

					for (auto connection : (firstActor ? components : otherConnections))
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

							overflow = true;
							return;
						}

						if (connection == connector && !firstActor)
						{
							continue;
						}

						if (!connection->IsConnected())
						{
							continue;
						}

						if (connection->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER ||
							GetConnectedPipeConnectionType(connection) == EPipeConnectionType::PCT_PRODUCER)
						{
							continue;
						}

						float previousLimit = out_limitedThroughput;
						collectOutput(
							resourceForm,
							connection->GetConnection(),
							out_requiredOutput,
							previousLimit,
							seenActors,
							connected,
							injectedItems,
							buildableSubsystem,
							level + 1,
							overflow,
							indent + TEXT("    "),
							timeout,
							machineStatusIncludeType
							);

						if (overflow)
						{
							return;
						}

						if (pipeline)
						{
							out_limitedThroughput = FMath::Min(out_limitedThroughput, previousLimit);
						}
						else if (firstConnection)
						{
							limitedThroughput = previousLimit;
							firstConnection = false;
						}
						else
						{
							limitedThroughput += previousLimit;
						}
					}

					if (!pipeline && !firstConnection)
					{
						out_limitedThroughput = FMath::Min(out_limitedThroughput, limitedThroughput);
					}

					auto pipeConnector = Cast<UFGPipeConnectionComponent>(connector);

					if (pipePump && pipeConnector->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER)
					{
						for (auto connection : components)
						{
							if (timeout < time(NULL))
							{
								EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating connectors!"));

								overflow = true;
								return;
							}

							if (connection == connector)
							{
								continue;
							}

							if (!connection->IsConnected() || connection->GetPipeConnectionType() != EPipeConnectionType::PCT_PRODUCER)
							{
								continue;
							}

							float previousLimit = 0;
							float discountedOutput = 0;

							TSet<AActor*> seenActorsCopy;

							auto tempInjectedItems = injectedItems;

							for (auto actor : seenActors)
							{
								if (timeout < time(NULL))
								{
									EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating seen actors!"));

									overflow = true;
									return;
								}

								seenActorsCopy.Add(actor.first);
							}

							collectInput(
								resourceForm,
								false,
								connection->GetConnection(),
								discountedOutput,
								previousLimit,
								seenActorsCopy,
								connected,
								tempInjectedItems,
								tempInjectedItems,
								buildableSubsystem,
								level + 1,
								overflow,
								indent + TEXT("    "),
								timeout,
								machineStatusIncludeType
								);

							if (overflow)
							{
								return;
							}

							if (discountedOutput > 0)
							{
								EC_LOG_Display_Condition(*indent, TEXT("Discounting "), discountedOutput, TEXT(" m³/minute"));

								out_requiredOutput -= discountedOutput;
							}
						}
					}

					EC_LOG_Display_Condition(*indent, *owner->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" m³/minute"));
				}

				connected.Add(buildable);

				return;
			}

			auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(owner);
			if (cargoPlatform)
			{
				addAllItemsToActor(seenActors, Cast<AFGBuildable>(cargoPlatform), injectedItems);

				TArray<UFGPipeConnectionComponent*> pipeConnections;
				cargoPlatform->GetComponents(pipeConnections);

				auto trackId = cargoPlatform->GetTrackGraphID();

				auto railroadSubsystem = AFGRailroadSubsystem::Get(owner->GetWorld());

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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

							overflow = true;
							return;
						}

						if (seenPlatforms.Contains(connectedPlatform))
						{
							// Loop detected
							break;
						}

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
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
								*indent,
								TEXT("    Station = "),
								*station->GetStationIdentifier()->GetStationName().ToString()
								);

							if (i == 0 && connectedPlatform->IsOrientationReversed() ||
								i == 1 && !connectedPlatform->IsOrientationReversed())
							{
								stationOffsets.insert(offsetDistance);
								EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), offsetDistance);
							}
							else
							{
								stationOffsets.insert(-offsetDistance);
								EC_LOG_Display_Condition(*indent, TEXT("        offset distance = "), -offsetDistance);
							}
						}

						if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
						{
							auto cargo = Cast<AFGBuildableTrainPlatformCargo>(connectedPlatform);
							if (cargo)
							{
								EC_LOG_Display(
									/**getTimeStamp(),*/
									*indent,
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
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating trains!"));

						overflow = true;
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
								*indent,
								TEXT("Train = "),
								*train->GetTrainName().ToString()
								);
						}
						else
						{
							EC_LOG_Display(
								/**getTimeStamp(),*/
								*indent,
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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

							overflow = true;
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
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating train stops!"));

							overflow = true;
							return;
						}

						if (!stop.Station || !stop.Station->GetStation())
						{
							continue;
						}

						EC_LOG_Display_Condition(
							/**getTimeStamp(),*/
							*indent,
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
								if (timeout < time(NULL))
								{
									EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while traversing platforms!"));

									overflow = true;
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

								auto adjustedOffsetDistance = i == 0 && !stop.Station->GetStation()->IsOrientationReversed()
								                              || i == 1 && stop.Station->GetStation()->IsOrientationReversed()
									                              ? offsetDistance
									                              : -offsetDistance;

								if (stationOffsets.find(adjustedOffsetDistance) == stationOffsets.end())
								{
									// Not on a valid offset. Skip
									continue;
								}

								addAllItemsToActor(seenActors, stopCargo, injectedItems);

								TArray<UFGPipeConnectionComponent*> cargoPipeConnections;
								stopCargo->GetComponents(cargoPipeConnections);

								pipeConnections.Append(
									cargoPipeConnections.FilterByPredicate(
										[&pipeConnections, stopCargo](UFGPipeConnectionComponent* connection)
										{
											if (pipeConnections.Contains(connection))
											{
												// Already in use
												return false;
											}

											if (stopCargo->GetIsInLoadMode())
											{
												// Loading
												return false;
											}

											if (connection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER)
											{
												// It is a producer connection
												return true;
											}

											return false;
										}
										)
									);
							}
						}
					}
				}

				bool firstConnection = true;
				float limitedThroughput = 0;

				for (auto connection : pipeConnections)
				{
					if (timeout < time(NULL))
					{
						EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating pipe connectors!"));

						overflow = true;
						return;
					}

					if (!connection->IsConnected() ||
						connection->GetPipeConnectionType() != EPipeConnectionType::PCT_PRODUCER)
					{
						continue;
					}

					float previousLimit = out_limitedThroughput;
					collectOutput(
						resourceForm,
						connection->GetConnection(),
						out_requiredOutput,
						previousLimit,
						seenActors,
						connected,
						injectedItems,
						buildableSubsystem,
						level + 1,
						overflow,
						indent + TEXT("    "),
						timeout,
						machineStatusIncludeType
						);

					if (overflow)
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

				out_limitedThroughput = FMath::Min(out_limitedThroughput, limitedThroughput);

				EC_LOG_Display_Condition(*indent, *owner->GetName(), TEXT(" limited at "), out_limitedThroughput, TEXT(" m³/minute"));

				connected.Add(Cast<AFGBuildable>(fluidIntegrant));

				return;
			}
		}

		{
			const auto generator = Cast<AFGBuildableGeneratorFuel>(owner);
			if (generator &&
				(generator->HasPower() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Unpowered)) &&
				(!generator->IsProductionPaused() || Has_EMachineStatusIncludeType(machineStatusIncludeType, EMachineStatusIncludeType::MSIT_Paused)))
			{
				if (injectedItems.Contains(generator->GetSupplementalResourceClass()) && !seenActors[generator].Contains(generator->GetSupplementalResourceClass()))
				{
					if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
					{
						EC_LOG_Display(
							/**getTimeStamp(),*/
							*indent,
							TEXT("Supplemental item = "),
							*UFGItemDescriptor::GetItemName(generator->GetSupplementalResourceClass()).ToString()
							);
						EC_LOG_Display(*indent, TEXT("Supplemental amount = "), generator->GetSupplementalConsumptionRateMaximum());
					}

					out_requiredOutput += generator->GetSupplementalConsumptionRateMaximum() * (
						(UFGItemDescriptor::GetForm(generator->GetSupplementalResourceClass()) == EResourceForm::RF_LIQUID ||
							UFGItemDescriptor::GetForm(generator->GetSupplementalResourceClass()) == EResourceForm::RF_GAS)
							? 60
							: 1);

					seenActors[generator].Add(generator->GetSupplementalResourceClass());
				}
				else
				{
					for (auto item : injectedItems)
					{
						if (timeout < time(NULL))
						{
							EC_LOG_Error_Condition(FUNCTIONSTR TEXT(": timeout while iterating injected items!"));

							overflow = true;
							return;
						}

						if (generator->IsValidFuel(item) && !seenActors[generator].Contains(item))
						{
							EC_LOG_Display_Condition(*indent, TEXT("Energy item = "), *UFGItemDescriptor::GetItemName(item).ToString());

							float energy = UFGItemDescriptor::GetEnergyValue(item);

							// if (UFGItemDescriptor::GetForm(out_injectedItem) == EResourceForm::RF_LIQUID)
							// {
							//     energy *= 1000;
							// }

							if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
							{
								EC_LOG_Display(*indent, TEXT("Energy = "), energy);
								EC_LOG_Display(*indent, TEXT("Current potential = "), generator->GetCurrentPotential());
								EC_LOG_Display(*indent, TEXT("Pending potential = "), generator->GetPendingPotential());
								EC_LOG_Display(*indent, TEXT("Power production capacity = "), generator->GetPowerProductionCapacity());
								EC_LOG_Display(*indent, TEXT("Default power production capacity = "), generator->GetDefaultPowerProductionCapacity());
							}

							float itemAmountPerMinute = 60 / (energy / generator->GetPowerProductionCapacity());

							if (resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS)
							{
								itemAmountPerMinute /= 1000;
							}

							EC_LOG_Display_Condition(
								/**getTimeStamp(),*/
								*indent,
								*generator->GetName(),
								TEXT(" consumes "),
								itemAmountPerMinute,
								TEXT(" "),
								*UFGItemDescriptor::GetItemName(item).ToString(),
								TEXT("/minute")
								);

							seenActors[generator].Add(item);
							out_requiredOutput += itemAmountPerMinute;

							break;
						}
					}
				}

				connected.Add(generator);

				return;
			}
		}

		addAllItemsToActor(seenActors, owner, injectedItems);

		// out_limitedThroughput = 0;

		if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		{
			dumpUnknownClass(indent, owner);
		}

		return;
	}
}

bool AEfficiencyCheckerLogic::inheritsFrom(AActor* owner, const FString& className)
{
	for (auto cls = owner->GetClass(); cls && cls != AActor::StaticClass(); cls = cls->GetSuperClass())
	{
		if (GetPathNameSafe(cls) == className)
		{
			return true;
		}
	}

	return false;
}

void AEfficiencyCheckerLogic::dumpUnknownClass(const FString& indent, AActor* owner)
{
	if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
	{
		EC_LOG_Display(*indent, TEXT("Unknown Class "), *GetPathNameSafe(owner->GetClass()));

		for (auto cls = owner->GetClass()->GetSuperClass(); cls && cls != AActor::StaticClass(); cls = cls->GetSuperClass())
		{
			EC_LOG_Display(*indent, TEXT("    - Super: "), *GetPathNameSafe(cls));
		}

		EC_LOG_Display(*indent, TEXT("Properties "), *GetPathNameSafe(owner->GetClass()));

		for (TFieldIterator<FProperty> property(owner->GetClass()); property; ++property)
		{
			EC_LOG_Display(
				*indent,
				TEXT("        - "),
				*property->GetName(),
				TEXT(" ("),
				*property->GetCPPType(),
				TEXT(" / Type: "),
				// *GetPathNameSafe(property->GetClass()->GetName()),
				*property->GetClass()->GetName(),
				TEXT(" / From: "),
				*GetPathNameSafe(property->GetOwnerClass()),
				TEXT(")")
				);

			auto floatProperty = CastField<FFloatProperty>(*property);
			if (floatProperty)
			{
				EC_LOG_Display(*indent, TEXT("            = "), floatProperty->GetPropertyValue_InContainer(owner));
			}

			auto intProperty = CastField<FIntProperty>(*property);
			if (intProperty)
			{
				EC_LOG_Display(*indent, TEXT("            = "), intProperty->GetPropertyValue_InContainer(owner));
			}

			auto boolProperty = CastField<FBoolProperty>(*property);
			if (boolProperty)
			{
				EC_LOG_Display(*indent, TEXT("            = "), boolProperty->GetPropertyValue_InContainer(owner) ? TEXT("true") : TEXT("false"));
			}

			auto structProperty = CastField<FStructProperty>(*property);
			if (structProperty && property->GetName() == TEXT("mFactoryTickFunction"))
			{
				auto factoryTick = structProperty->ContainerPtrToValuePtr<FFactoryTickFunction>(owner);
				if (factoryTick)
				{
					EC_LOG_Display(*indent, TEXT("            - Tick Interval = "), factoryTick->TickInterval);
				}
			}

			auto strProperty = CastField<FStrProperty>(*property);
			if (strProperty)
			{
				EC_LOG_Display(*indent, TEXT("            = "), *strProperty->GetPropertyValue_InContainer(owner));
			}

			auto classProperty = CastField<FClassProperty>(*property);
			if (classProperty)
			{
				auto ClassObject = Cast<UClass>(classProperty->GetPropertyValue_InContainer(owner));
				EC_LOG_Display(*indent, TEXT("            - Class = "), *GetPathNameSafe(ClassObject));
			}
		}
	}
}

void AEfficiencyCheckerLogic::DumpInformation(AActor* worldContext, TSubclassOf<UFGItemDescriptor> itemDescriptorClass)
{
	// if (configuration.dumpConnections)
	{
		if (!itemDescriptorClass)
		{
			return;
		}

		auto className = GetFullNameSafe(itemDescriptorClass);

		EC_LOG_Display(TEXT("Dumping "), *className);

		auto itemDescriptor = Cast<UFGItemDescriptor>(itemDescriptorClass->GetDefaultObject());
		if (!itemDescriptor)
		{
			EC_LOG_Display(TEXT("    Equipment small icon = "), *GetPathNameSafe(itemDescriptor->mSmallIcon));
			EC_LOG_Display(TEXT("    Equipment big icon = "), *GetPathNameSafe(itemDescriptor->mPersistentBigIcon));
			EC_LOG_Display(TEXT("    Equipment conveyor mesh = "), *GetPathNameSafe(itemDescriptor->mConveyorMesh));
			EC_LOG_Display(TEXT("    Equipment category = "), *GetPathNameSafe(itemDescriptor->mCategory));
		}

		auto equipmentDescriptor = Cast<UFGEquipmentDescriptor>(itemDescriptor);
		if (!equipmentDescriptor)
		{
			EC_LOG_Display(TEXT("    Class "), *className, TEXT(" is not an Equipment Descriptor"));

			return;
		}

		EC_LOG_Display(TEXT("    Equipment stack size = "), *getEnumItemName(TEXT("EStackSize"), (int)equipmentDescriptor->mStackSize));

		EC_LOG_Display(TEXT("    Equipment class = "), *GetPathNameSafe(equipmentDescriptor->mEquipmentClass));

		if (!equipmentDescriptor->mEquipmentClass)
		{
			return;
		}

		// auto equipment = Cast<AFGEquipment>(equipmentDescriptor->mEquipmentClass->GetDefaultObject());

		auto equipment = Cast<AFGEquipment>(
			worldContext->GetWorld()->SpawnActor(
				equipmentDescriptor->mEquipmentClass
				)
			);

		EC_LOG_Display(TEXT("    Equipment attachment = "), *GetPathNameSafe(equipment->mAttachmentClass));
		EC_LOG_Display(TEXT("    Equipment secondary attachment = "), *GetPathNameSafe(equipment->mSecondaryAttachmentClass));
		EC_LOG_Display(TEXT("    Equipment slot = "), *getEnumItemName(TEXT("EEquipmentSlot"), (int)equipment->mEquipmentSlot));
		EC_LOG_Display(TEXT("    Equipment attachment socket = "), *equipment->mAttachSocket.ToString());
		EC_LOG_Display(TEXT("    Equipment arm animation = "), *getEnumItemName(TEXT("EArmEquipment"), (int)equipment->GetArmsAnimation()));
		EC_LOG_Display(TEXT("    Equipment back animation = "), *getEnumItemName(TEXT("EBackEquipment"), (int)equipment->GetBackAnimation()));
		EC_LOG_Display(TEXT("    Equipment equip sound = "), *GetPathNameSafe(equipment->mEquipSound));
		EC_LOG_Display(TEXT("    Equipment unequip sound = "), *GetPathNameSafe(equipment->mUnequipSound));
		EC_LOG_Display(TEXT("    Equipment widget = "), *GetPathNameSafe(equipment->mEquipmentWidget));
		// EC_LOG_Display(TEXT("    Equipment use default primary fire = "), equipment->CanDoDefaultPrimaryFire() ? TEXT("true") : TEXT("false"));
		EC_LOG_Display(TEXT("    Equipment idle pose animation = "), *GetPathNameSafe(equipment->GetIdlePoseAnimation()));
		EC_LOG_Display(TEXT("    Equipment idle pose animation 3p = "), *GetPathNameSafe(equipment->GetIdlePoseAnimation3p()));
		EC_LOG_Display(TEXT("    Equipment crouch pose animation 3p = "), *GetPathNameSafe(equipment->GetCrouchPoseAnimation3p()));
		EC_LOG_Display(TEXT("    Equipment slide pose animation 3p = "), *GetPathNameSafe(equipment->GetSlidePoseAnimation3p()));
		EC_LOG_Display(TEXT("    Equipment attachmend idle AO = "), *GetPathNameSafe(equipment->GetAttachmentIdleAO()));

		auto& components = equipment->GetComponents();

		for (auto component : components)
		{
			EC_LOG_Display(TEXT("    Component Class = "), *GetFullNameSafe(component->GetClass()));

			if (auto scene = Cast<USceneComponent>(component))
			{
				EC_LOG_Display(TEXT("        Location = "), *scene->GetComponentLocation().ToString());
				EC_LOG_Display(TEXT("        Rotation = "), *scene->GetComponentRotation().ToString());
				EC_LOG_Display(TEXT("        Scale = "), *scene->GetComponentScale().ToString());
			}

			if (auto skeletalMesh = Cast<USkeletalMeshComponent>(component))
			{
				EC_LOG_Display(TEXT("        Animation Mode = "), *getEnumItemName(TEXT("EAnimationMode"), skeletalMesh->GetAnimationMode()));
				EC_LOG_Display(TEXT("        Anim Class = "), *GetPathNameSafe(skeletalMesh->GetAnimClass()));
				EC_LOG_Display(TEXT("        Disable Post Process Blueprint = "), skeletalMesh->GetDisablePostProcessBlueprint() ? TEXT("true") : TEXT("false"));
				EC_LOG_Display(TEXT("        Global Anim Rate Scale = "), skeletalMesh->GlobalAnimRateScale);
				EC_LOG_Display(TEXT("        Pause Anims = "), skeletalMesh->bPauseAnims ? TEXT("true") : TEXT("false"));
				EC_LOG_Display(TEXT("        Use Ref Pose On Init Anim = "), skeletalMesh->bUseRefPoseOnInitAnim ? TEXT("true") : TEXT("false"));
				EC_LOG_Display(TEXT("        Skeletal Mesh = "), *GetPathNameSafe(skeletalMesh->SkeletalMesh));
			}
		}

		equipment->Destroy();

		EC_LOG_Display(TEXT(" Dump done"));
		EC_LOG_Display(TEXT("===="));
	}
}

bool AEfficiencyCheckerLogic::IsValidBuildable(AFGBuildable* newBuildable)
{
	if (!newBuildable)
	{
		return false;
	}

	if (auto checker = Cast<AEfficiencyCheckerBuilding>(newBuildable))
	{
		addEfficiencyBuilding(checker);

		return true;
	}
	if (Cast<AFGBuildableManufacturer>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableResourceExtractor>(newBuildable))
	{
		return true;
	}
	if (GetPathNameSafe(newBuildable->GetClass()).EndsWith("/UndergroundBelts/Build/Build_UndergroundSplitterInput.Build_UndergroundSplitterInput_C"))
	{
		if (auto underGroundBelt = Cast<AFGBuildableStorage>(newBuildable))
		{
			addUndergroundInputBelt(underGroundBelt);

			return true;
		}
	}
	if (Cast<AFGBuildableStorage>(newBuildable))
	{
		return true;
	}
	if (auto belt = Cast<AFGBuildableConveyorBelt>(newBuildable))
	{
		addBelt(belt);

		return true;
	}
	if (Cast<AFGBuildableConveyorBase>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableConveyorAttachment>(newBuildable))
	{
		return true;
	}
	if (auto pipe = Cast<AFGBuildablePipeline>(newBuildable))
	{
		addPipe(pipe);

		return true;
	}
	if (Cast<AFGBuildablePipelineAttachment>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableTrainPlatform>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableRailroadStation>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableDockingStation>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableGeneratorFuel>(newBuildable))
	{
		return true;
	}
	if (Cast<AFGBuildableGeneratorNuclear>(newBuildable))
	{
		return true;
	}
	if (!configuration.ignoreStorageTeleporter &&
		GetPathNameSafe(newBuildable->GetClass()).EndsWith(TEXT("/StorageTeleporter/Buildables/ItemTeleporter/ItemTeleporter_Build.ItemTeleporter_Build_C")))
	{
		addTeleporter(newBuildable);

		return true;
	}

	return false;
}

void AEfficiencyCheckerLogic::addEfficiencyBuilding(class AEfficiencyCheckerBuilding* checker)
{
	FScopeLock ScopeLock(&eclCritical);
	allEfficiencyBuildings.Add(checker);

	checker->OnEndPlay.Add(removeEffiencyBuildingDelegate);
}

void AEfficiencyCheckerLogic::removeEfficiencyBuilding(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allEfficiencyBuildings.Remove(Cast<AEfficiencyCheckerBuilding>(actor));

	actor->OnEndPlay.Remove(removeEffiencyBuildingDelegate);
}

void AEfficiencyCheckerLogic::addBelt(AFGBuildableConveyorBelt* belt)
{
	FScopeLock ScopeLock(&eclCritical);
	allBelts.Add(belt);

	belt->OnEndPlay.Add(removeBeltDelegate);
}

void AEfficiencyCheckerLogic::removeBelt(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allBelts.Remove(Cast<AFGBuildableConveyorBelt>(actor));

	actor->OnEndPlay.Remove(removeBeltDelegate);
}

void AEfficiencyCheckerLogic::addPipe(AFGBuildablePipeline* pipe)
{
	FScopeLock ScopeLock(&eclCritical);
	allPipes.Add(pipe);

	pipe->OnEndPlay.Add(removePipeDelegate);
}

void AEfficiencyCheckerLogic::removePipe(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allPipes.Remove(Cast<AFGBuildablePipeline>(actor));

	actor->OnEndPlay.Remove(removePipeDelegate);
}

void AEfficiencyCheckerLogic::addTeleporter(AFGBuildable* teleporter)
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Add(teleporter);

	teleporter->OnEndPlay.Add(removeTeleporterDelegate);
}

void AEfficiencyCheckerLogic::removeTeleporter(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Remove(Cast<AFGBuildable>(actor));

	actor->OnEndPlay.Remove(removeTeleporterDelegate);
}

void AEfficiencyCheckerLogic::addUndergroundInputBelt(AFGBuildableStorage* undergroundInputBelt)
{
	FScopeLock ScopeLock(&eclCritical);
	allUndergroundInputBelts.Add(undergroundInputBelt);

	undergroundInputBelt->OnEndPlay.Add(removeUndergroundInputBeltDelegate);
}

void AEfficiencyCheckerLogic::removeUndergroundInputBelt(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allUndergroundInputBelts.Remove(Cast<AFGBuildableStorage>(actor));

	actor->OnEndPlay.Remove(removeUndergroundInputBeltDelegate);
}

float AEfficiencyCheckerLogic::getPipeSpeed(AFGBuildablePipeline* pipe)
{
	if (!pipe)
	{
		return 0;
	}

	return UFGBlueprintFunctionLibrary::RoundFloatWithPrecision(pipe->GetFlowLimit() * 60, 4);
}

void AEfficiencyCheckerLogic::setConfiguration(const FEfficiencyChecker_ConfigStruct& in_configuration)
{
	configuration = in_configuration;

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	EC_LOG_Display(TEXT("StartupModule"));

	if (configuration.updateTimeout <= 0)
	{
		configuration.updateTimeout = 15;
	}

	EC_LOG_Display(TEXT("autoUpdate = "), configuration.autoUpdate ? TEXT("true") : TEXT("false"));
	EC_LOG_Display(TEXT("autoUpdateTimeout = "), configuration.autoUpdateTimeout);
	EC_LOG_Display(TEXT("autoUpdateDistance = "), configuration.autoUpdateDistance);
	EC_LOG_Display(TEXT("logLevel = "), configuration.logLevel);
	EC_LOG_Display(TEXT("ignoreStorageTeleporter = "), configuration.ignoreStorageTeleporter ? TEXT("true") : TEXT("false"));
	EC_LOG_Display(TEXT("updateTimeout = "), configuration.updateTimeout);

	if (configuration.autoUpdate)
	{
		EC_LOG_Display(TEXT("Hooking AFGBuildableFactory::SetPendingPotential"));

		{
			void* ObjectInstance = GetMutableDefault<AFGBuildableFactory>();

			SUBSCRIBE_METHOD_VIRTUAL_AFTER(
				AFGBuildableFactory::SetPendingPotential,
				ObjectInstance,
				[](AFGBuildableFactory * factory, float potential) {
				AEfficiencyCheckerBuilding::setPendingPotentialCallback(factory, potential);
				}
				);
		}

		{
			void* ObjectInstance = GetMutableDefault<AFGBuildableFrackingActivator>();

			SUBSCRIBE_METHOD_VIRTUAL_AFTER(
				AFGBuildableFrackingActivator::SetPendingPotential,
				ObjectInstance,
				[](AFGBuildableFrackingActivator * factory, float potential) {
				AEfficiencyCheckerBuilding::setPendingPotentialCallback(factory, potential);
				}
				);
		}

		{
			void* ObjectInstance = GetMutableDefault<AFGBuildableGeneratorFuel>();

			SUBSCRIBE_METHOD_VIRTUAL_AFTER(
				AFGBuildableGeneratorFuel::SetPendingPotential,
				ObjectInstance,
				[](AFGBuildableGeneratorFuel * factory, float potential) {
				AEfficiencyCheckerBuilding::setPendingPotentialCallback(factory, potential);
				}
				);
		}
	}

	EC_LOG_Display(TEXT("==="));
}

bool AEfficiencyCheckerLogic::IsAutoUpdateEnabled()
{
	return configuration.autoUpdate;
}

int AEfficiencyCheckerLogic::GetLogLevelECM()
{
	return configuration.logLevel;
}

float AEfficiencyCheckerLogic::GetAutoUpdateTimeout()
{
	return configuration.autoUpdateTimeout;
}

float AEfficiencyCheckerLogic::GetAutoUpdateDistance()
{
	return configuration.autoUpdateDistance;
}

EPipeConnectionType AEfficiencyCheckerLogic::GetConnectedPipeConnectionType(class UFGPipeConnectionComponent* component)
{
	auto connectionType = EPipeConnectionType::PCT_ANY;

	if (component)
	{
		connectionType = component->GetPipeConnectionType();

		if (connectionType == EPipeConnectionType::PCT_ANY && component->IsConnected() && component->GetConnection())
		{
			connectionType = component->GetConnection()->GetPipeConnectionType();
		}
	}

	return connectionType;
}

void AEfficiencyCheckerLogic::collectUndergroundBeltsComponents
(
	AFGBuildableStorage* undergroundBelt,
	TSet<UFGFactoryConnectionComponent*>& components,
	TSet<AActor*>& actors
)
{
	auto outputsProperty = CastField<FArrayProperty>(undergroundBelt->GetClass()->FindPropertyByName("Outputs"));
	if (outputsProperty)
	{
		FScriptArrayHelper arrayHelper(outputsProperty, outputsProperty->ContainerPtrToValuePtr<void>(undergroundBelt));
		auto arrayObjectProperty = CastField<FObjectProperty>(outputsProperty->Inner);

		for (auto x = 0; x < arrayHelper.Num(); x++)
		{
			void* ObjectContainer = arrayHelper.GetRawPtr(x);
			auto output = Cast<AFGBuildableFactory>(arrayObjectProperty->GetObjectPropertyValue(ObjectContainer));
			if (output)
			{
				actors.Add(output);

				components.Append(
					output->GetConnectionComponents().FilterByPredicate(
						[&components](UFGFactoryConnectionComponent* connection)
						{
							return !components.Contains(connection) && // Not in use already
								connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT; // Is output connection
						}
						)
					);
			}
		}
	}
	else
	{
		for (auto inputUndergroundBelt : singleton->allUndergroundInputBelts)
		{
			outputsProperty = CastField<FArrayProperty>(inputUndergroundBelt->GetClass()->FindPropertyByName("Outputs"));
			if (outputsProperty)
			{
				FScriptArrayHelper arrayHelper(outputsProperty, outputsProperty->ContainerPtrToValuePtr<void>(inputUndergroundBelt));
				auto arrayObjectProperty = CastField<FObjectProperty>(outputsProperty->Inner);

				auto found = false;

				for (auto x = 0; !found && x < arrayHelper.Num(); x++)
				{
					void* ObjectContainer = arrayHelper.GetRawPtr(x);
					found = undergroundBelt == arrayObjectProperty->GetObjectPropertyValue(ObjectContainer);
				}

				if (found)
				{
					actors.Add(inputUndergroundBelt);

					components.Append(
						inputUndergroundBelt->GetConnectionComponents().FilterByPredicate(
							[&components](UFGFactoryConnectionComponent* connection)
							{
								return !components.Contains(connection) && // Not in use already
									connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT; // Is output connection
							}
							)
						);
				}
			}
		}
	}
}


#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
