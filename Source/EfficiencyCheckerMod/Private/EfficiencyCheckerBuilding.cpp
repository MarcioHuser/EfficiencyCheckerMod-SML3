// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo

#include "EfficiencyCheckerBuilding.h"

#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Components/WidgetComponent.h"
#include "EfficiencyCheckerRCO.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "Logic/EfficiencyCheckerLogic.h"
#include "Util/ECMOptimize.h"
#include "Util/ECMLogging.h"

#include <map>

#include "Logic/CollectSettings.h"
#include "Logic/EfficiencyCheckerLogic2.h"
#include "Net/UnrealNetwork.h"
#include "Async/Async.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystems/CommonInfoSubsystem.h"
#include "Util/EfficiencyCheckerConfiguration.h"

#ifndef OPTIMIZE
#pragma optimize("", off)
#endif

// Sets default values
AEfficiencyCheckerBuilding::AEfficiencyCheckerBuilding()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PrimaryActorTick.SetTickFunctionEnable(false);

	// //// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	// mFactoryTickFunction.bCanEverTick = true;
	// mFactoryTickFunction.bAllowTickOnDedicatedServer = true;
	// mFactoryTickFunction.bStartWithTickEnabled = false;
	//
	// mFactoryTickFunction.SetTickFunctionEnable(false);
}

// Called when the game starts or when spawned
void AEfficiencyCheckerBuilding::BeginPlay()
{
	_TAG_NAME = GetName() + TEXT(": ");

	EC_LOG_Display_Condition(getTagName(), TEXT("BeginPlay"));

	Super::BeginPlay();

	auto arrows = GetComponentsByTag(UStaticMeshComponent::StaticClass(), TEXT("DirectionArrow"));
	for (auto arrow : arrows)
	{
		// Cast<UStaticMeshComponent>(arrow)->SetVisibilitySML(false);

		arrow->DestroyComponent();
	}

	TInlineComponentArray<UWidgetComponent*> widgets(this, true);
	for (auto widget : widgets)
	{
		widget->SetVisibility(true);
	}

	if (HasAuthority())
	{
		if (innerPipelineAttachment && pipelineToSplit)
		{
			auto fluidType = pipelineToSplit->GetFluidDescriptor();

			if (150 < pipelineSplitOffset && pipelineSplitOffset < pipelineToSplit->GetLength() - 150)
			{
				// Split into two pipes
				auto newPipes = AFGBuildablePipeline::Split(pipelineToSplit, pipelineSplitOffset, false, innerPipelineAttachment);

				if (newPipes.Num() > 1)
				{
					auto attachmentPipeConnections = innerPipelineAttachment->GetPipeConnections();

					const auto pipe0OutputCoincident = (
						newPipes[0]->GetPipeConnection1()->GetComponentRotation().Vector() | attachmentPipeConnections[0]->GetComponentRotation().Vector()
					) >= 0;

					// Must connect to the one point the other way around
					newPipes[0]->GetPipeConnection1()->SetConnection(
						pipe0OutputCoincident
							? attachmentPipeConnections[1]
							: attachmentPipeConnections[0]
						);

					// Must connect to the one point the other way around
					newPipes[1]->GetPipeConnection0()->SetConnection(
						pipe0OutputCoincident
							? attachmentPipeConnections[0]
							: attachmentPipeConnections[1]
						);
				}
			}
			else
			{
				// Attach to the nearest edge of the pipe
				const auto closestPipeConnection = pipelineSplitOffset <= 150 ? pipelineToSplit->GetPipeConnection0() : pipelineToSplit->GetPipeConnection1();

				for (auto attachmentConnection : innerPipelineAttachment->GetPipeConnections())
				{
					if (FVector::Dist(attachmentConnection->GetConnectorLocation(), closestPipeConnection->GetConnectorLocation()) < 1)
					{
						closestPipeConnection->SetConnection(attachmentConnection);

						break;
					}
				}
			}

			AFGPipeSubsystem::Get(GetWorld())->TrySetNetworkFluidDescriptor(innerPipelineAttachment->GetPipeConnections()[0]->GetPipeNetworkID(), fluidType);
		}

		pipelineToSplit = nullptr;

		if ((AEfficiencyCheckerConfiguration::configuration.autoUpdate && autoUpdateMode == EAutoUpdateType::AUT_USE_DEFAULT) ||
			autoUpdateMode == EAutoUpdateType::AUT_ENABLED)
		{
			lastUpdated = GetWorld()->GetTimeSeconds();
			updateRequested = lastUpdated + AEfficiencyCheckerConfiguration::configuration.autoUpdateTimeout; // Give a timeout
			SetActorTickEnabled(true);
			SetActorTickInterval(AEfficiencyCheckerConfiguration::configuration.autoUpdateTimeout);
			// mFactoryTickFunction.SetTickFunctionEnable(true);
			// mFactoryTickFunction.TickInterval = autoUpdateTimeout;

			addOnDestroyBindings(pendingBuildables);
			addOnDestroyBindings(connectedBuildables);
			addOnRecipeChangedBindings(connectedBuildables);
			addOnSortRulesChangedDelegateBindings(connectedBuildables);

			checkTick_ = true;
			//checkFactoryTick_ = true;
		}
		else if (CreationTime < GetWorld()->GetTimeSeconds())
		{
			SetActorTickEnabled(true);
			SetActorTickInterval(0);

			doUpdateItem = true;
		}
		else
		{
			updateRequested = GetWorld()->GetTimeSeconds();
			SetActorTickEnabled(true);
			SetActorTickInterval(0);
			mustUpdate_ = true;
		}
	}
}

void AEfficiencyCheckerBuilding::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	Super::EndPlay(endPlayReason);

	if (AEfficiencyCheckerLogic::singleton)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Removing from list!"));

		FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);
		AEfficiencyCheckerLogic::singleton->allEfficiencyBuildings.Remove(this);
	}

	if (HasAuthority())
	{
		if (innerPipelineAttachment && endPlayReason == EEndPlayReason::Destroyed)
		{
			auto attachmentPipeConnections = innerPipelineAttachment->GetPipeConnections();

			const auto pipeConnection1 = attachmentPipeConnections.Num() > 0 && attachmentPipeConnections[0]->IsConnected()
				                             ? attachmentPipeConnections[0]->GetPipeConnection()
				                             : nullptr;
			const auto pipeConnection2 = attachmentPipeConnections.Num() > 1 && attachmentPipeConnections[1]->IsConnected()
				                             ? attachmentPipeConnections[1]->GetPipeConnection()
				                             : nullptr;

			auto fluidType = pipeConnection1 ? pipeConnection1->GetFluidDescriptor() : nullptr;
			if (!fluidType)
			{
				fluidType = pipeConnection2 ? pipeConnection2->GetFluidDescriptor() : nullptr;
			}

			// Remove the connections
			for (auto connection : attachmentPipeConnections)
			{
				connection->ClearConnection();
			}

			if (pipeConnection1 && pipeConnection2)
			{
				auto dist = FVector::Dist(pipeConnection1->GetConnectorLocation(), pipeConnection2->GetConnectorLocation());

				if (dist <= 1)
				{
					// Connect the pipes
					pipeConnection1->SetConnection(pipeConnection2);

					// Merge the pipes
					TArray<AFGBuildablePipeline*> pipelines;
					pipelines.Add(Cast<AFGBuildablePipeline>(pipeConnection1->GetOwner()));
					pipelines.Add(Cast<AFGBuildablePipeline>(pipeConnection2->GetOwner()));

					auto newPipe = AFGBuildablePipeline::Merge(pipelines);

					auto pipeSubsystem = AFGPipeSubsystem::Get(GetWorld());
					pipeSubsystem->TrySetNetworkFluidDescriptor(pipeConnection1->GetPipeNetworkID(), fluidType);
				}
			}

			innerPipelineAttachment->Destroy();

			innerPipelineAttachment = nullptr;
		}

		removeOnDestroyBindings(pendingBuildables);
		removeOnDestroyBindings(connectedBuildables);
		removeOnRecipeChangedBindings(connectedBuildables);
		removeOnSortRulesChangedDelegateBindings(connectedBuildables);

		pendingBuildables.Empty();
		connectedBuildables.Empty();
	}
}

void AEfficiencyCheckerBuilding::Tick(float dt)
{
	Super::Tick(dt);

	if (HasAuthority())
	{
		if (checkTick_)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Ticking"));
		}

		if (doUpdateItem)
		{
			UpdateItem(
				injectedInput,
				limitedThroughput,
				requiredOutput,
				injectedItems,
				TArray<FProductionDetail>(),
				TArray<FProductionDetail>(),
				overflow
				);
			SetActorTickEnabled(false);
		}
		else if (lastUpdated < updateRequested && updateRequested <= GetWorld()->GetTimeSeconds())
		{
			auto playerIt = GetWorld()->GetPlayerControllerIterator();
			for (; playerIt; ++playerIt)
			{
				if (checkTick_)
				{
					EC_LOG_Display_Condition(*getTagName(), TEXT("Player Controller"));
				}

				const auto pawn = (*playerIt)->GetPawn();
				if (pawn)
				{
					auto playerTranslation = pawn->GetActorLocation();

					if (!((AEfficiencyCheckerConfiguration::configuration.autoUpdate && autoUpdateMode == EAutoUpdateType::AUT_USE_DEFAULT) ||
							autoUpdateMode == EAutoUpdateType::AUT_ENABLED) ||
						FVector::Dist(playerTranslation, GetActorLocation()) <= AEfficiencyCheckerConfiguration::configuration.autoUpdateDistance)
					{
						EC_LOG_Display_Condition(*getTagName(), TEXT("Last Tick"));
						// EC_LOG_Display_Condition(*getTagName(), TEXT("Player Pawn"));
						// EC_LOG_Display_Condition(*getTagName(), TEXT("Translation X = "), playerTranslation.X, TEXT(" / Y = "), playerTranslation.Y,TEXT( " / Z = "), playerTranslation.Z);

						// Check if has pending buildings
						if (!mustUpdate_)
						{
							if (connectedBuildables.Num())
							{
								for (auto pending : pendingBuildables)
								{
									// If building connects to any of connections, it must be updated
									TInlineComponentArray<UFGFactoryConnectionComponent*> components;
									pending->GetComponents(components);

									for (auto component : components)
									{
										const auto connectionComponent = Cast<UFGFactoryConnectionComponent>(component);
										if (!connectionComponent->IsConnected())
										{
											continue;
										}

										if (connectedBuildables.Contains(Cast<AFGBuildable>(connectionComponent->GetConnection()->GetOwner())))
										{
											EC_LOG_Display_Condition(
												*getTagName(),
												TEXT("New building "),
												playerTranslation.X,
												TEXT(" connected to known building "),
												*GetPathNameSafe(connectionComponent->GetConnection()->GetOwner())
												);

											mustUpdate_ = true;
											break;
										}
									}

									if (mustUpdate_)
									{
										break;
									}
								}
							}
							else
							{
								mustUpdate_ = true;
							}
						}

						removeOnDestroyBindings(pendingBuildables);

						pendingBuildables.Empty();

						if (mustUpdate_)
						{
							// Recalculate connections
							Server_UpdateConnectedProduction(true, false, 0, true, false, 0);
						}

						SetActorTickEnabled(false);
						// mFactoryTickFunction.SetTickFunctionEnable(false);

						break;
					}
				}
			}
		}

		checkTick_ = false;
	}
}

// // Called every frame
// void AEfficiencyCheckerBuilding::Factory_Tick(float dt)
// {
//     Super::Factory_Tick(dt);
//
//     if (HasAuthority())
//     {
//         if (checkFactoryTick_)
//         {
//             EC_LOG_Display_Condition(*getTagName(), TEXT("Factory Ticking"));
//             checkFactoryTick_ = false;
//         }
//
//         if (lastUpdated < updateRequested && updateRequested <= GetWorld()->GetTimeSeconds())
//         {
//             EC_LOG_Display_Condition(*getTagName(), TEXT("Last Factory Tick"));
//
//             //Server_UpdateConnectedProduction(injectedInput, limitedThroughput, requiredOutput, injectedItem, dumpConnections);
//
//             //SetActorTickEnabled(false);
//             //mFactoryTickFunction.SetTickFunctionEnable(false);
//         }
//     }
// }

void AEfficiencyCheckerBuilding::SetCustomInjectedInput(bool enabled, float value)
{
	if (HasAuthority())
	{
		Server_SetCustomInjectedInput(enabled, value);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling SetCustomInjectedInput at server"));

			rco->SetCustomInjectedInputRPC(this, enabled, value);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_SetCustomInjectedInput(bool enabled, float value)
{
	if (HasAuthority())
	{
		customInjectedInput = enabled;
		injectedInput = customInjectedInput ? value : 0;
	}
}

void AEfficiencyCheckerBuilding::SetCustomRequiredOutput(bool enabled, float value)
{
	if (HasAuthority())
	{
		Server_SetCustomRequiredOutput(enabled, value);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling SetCustomRequiredOutput at server"));

			rco->SetCustomRequiredOutputRPC(this, enabled, value);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_SetCustomRequiredOutput(bool enabled, float value)
{
	if (HasAuthority())
	{
		customRequiredOutput = enabled;
		requiredOutput = customRequiredOutput ? value : 0;
	}
}

void AEfficiencyCheckerBuilding::SetAutoUpdateMode(EAutoUpdateType in_autoUpdateMode)
{
	if (HasAuthority())
	{
		Server_SetAutoUpdateMode(in_autoUpdateMode);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling SetAutoUpdateMode at server"));

			rco->SetAutoUpdateModeRPC(this, in_autoUpdateMode);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_SetAutoUpdateMode(EAutoUpdateType in_autoUpdateMode)
{
	if (HasAuthority())
	{
		this->autoUpdateMode = in_autoUpdateMode;
	}
}

void AEfficiencyCheckerBuilding::SetMachineStatusIncludeType(int32 in_machineStatusIncludeType)
{
	if (HasAuthority())
	{
		Server_SetMachineStatusIncludeType(in_machineStatusIncludeType);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling SetMachineStatusIncludeType at server"));

			rco->SetMachineStatusIncludeTypeRPC(this, in_machineStatusIncludeType);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_SetMachineStatusIncludeType(int32 in_machineStatusIncludeType)
{
	if (HasAuthority())
	{
		this->machineStatusIncludeType = in_machineStatusIncludeType;
	}
}

void AEfficiencyCheckerBuilding::UpdateBuilding(AFGBuildable* newBuildable)
{
	if (HasAuthority())
	{
		Server_UpdateBuilding(newBuildable);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling UpdateBuilding at server"));

			rco->UpdateBuildingRPC(this, newBuildable);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_UpdateBuilding(AFGBuildable* newBuildable)
{
	if (!IsInGameThread())
	{
		AsyncTask(
			ENamedThreads::GameThread,
			[this, newBuildable]()
			{
				Server_UpdateBuilding(newBuildable);
			}
			);

		return;
	}

	if (HasAuthority())
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT(" OnUpdateBuilding"));

		if (newBuildable)
		{
			Server_AddPendingBuilding(newBuildable);
			// Trigger event to start listening for possible dismantle before checking the usage
			AddOnDestroyBinding(newBuildable);
		}

		// Trigger specific building
		updateRequested = GetWorld()->GetRealTimeSeconds() + AEfficiencyCheckerConfiguration::configuration.autoUpdateTimeout;
		// Give a 5 seconds timeout

		EC_LOG_Display_Condition(TEXT("    Updating "), *GetName());

		SetActorTickEnabled(true);
		SetActorTickInterval(AEfficiencyCheckerConfiguration::configuration.autoUpdateTimeout);
		// mFactoryTickFunction.SetTickFunctionEnable(true);
		// mFactoryTickFunction.TickInterval = autoUpdateTimeout;

		checkTick_ = true;
		//checkFactoryTick_ = true;
		mustUpdate_ = true;
	}
	else
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT(" OnUpdateBuilding - no authority"));
	}

	EC_LOG_Display_Condition(TEXT("===="));
}

void AEfficiencyCheckerBuilding::UpdateBuildings(AFGBuildable* newBuildable)
{
	EC_LOG_Display_Condition(TEXT("EfficiencyCheckerBuilding: UpdateBuildings"));

	if (newBuildable)
	{
		EC_LOG_Display_Condition(TEXT("    New buildable: "), *newBuildable->GetName());

		//TArray<UActorComponent*> components = newBuildable->GetComponentsByClass(UFGFactoryConnectionComponent::StaticClass());
		//for (auto component : components) {
		//	UFGFactoryConnectionComponent* connectionComponent = Cast<UFGFactoryConnectionComponent>(component);

		//	if (connectionComponent->IsConnected()) {
		//		EC_LOG_Display_Condition(TEXT("        - "),  *component->GetName(), TEXT(" is connected to"), * connectionComponent->GetConnection()->GetOwner()->GetName());
		//	}
		//	else {
		//		EC_LOG_Display_Condition(TEXT( "        - "),  *component->GetName(), TEXT(" is not connected"));
		//	}
		//}
	}

	// Update all EfficiencyCheckerBuildings
	FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);

	// Trigger all buildings
	for (auto efficiencyBuilding : AEfficiencyCheckerLogic::singleton->allEfficiencyBuildings)
	{
		efficiencyBuilding->UpdateBuilding(newBuildable);
	}

	EC_LOG_Display_Condition(TEXT("===="));
}

// UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "EfficiencyChecker")
void AEfficiencyCheckerBuilding::GetConnectedProduction
(
	float& out_injectedInput,
	float& out_limitedThroughput,
	float& out_requiredOutput,
	TSet<TSubclassOf<UFGItemDescriptor>>& out_injectedItems,
	TSet<AFGBuildable*>& connected,
	TArray<FProductionDetail>& producers,
	TArray<FProductionDetail>& consumers,
	bool& in_overflow,
	bool includeProductionDetails
)
{
	EC_LOG_Display_Condition(*getTagName(), *getTagName(), TEXT("GetConnectedProduction"));

	CollectSettings collectSettings;
	collectSettings.SetLimitedThroughputPtr(&out_limitedThroughput);
	// collectSettings.SetInjectedItemsPtr(&out_injectedItems);
	collectSettings.SetConnectedPtr(&connected);
	collectSettings.SetMachineStatusIncludeType(machineStatusIncludeType);
	collectSettings.SetResourceForm(resourceForm);
	collectSettings.SetOverflowPtr(&in_overflow);

	//const FString indent(TEXT("    "));
	collectSettings.SetIndent(TEXT("    "));

	//const auto buildableSubsystem = AFGBuildableSubsystem::Get(GetWorld());
	collectSettings.SetBuildableSubsystem(AFGBuildableSubsystem::Get(GetWorld()));

	UFGConnectionComponent* inputConnector = nullptr;
	UFGConnectionComponent* outputConnector = nullptr;

	// TSet<TSubclassOf<UFGItemDescriptor>> restrictedItems;

	float initialThroughtputLimit = 0;
	// in_overflow = false;
	collectSettings.SetOverflow(false);

	auto commonInfoSubsystem = ACommonInfoSubsystem::Get(GetWorld());

	if (innerPipelineAttachment)
	{
		auto attachmentPipeConnections = innerPipelineAttachment->GetPipeConnections();

		TSubclassOf<UFGItemDescriptor> fluidItem;

		bool firstConnector = true;

		for (auto pipeConnection : attachmentPipeConnections)
		{
			if (!pipeConnection->IsConnected())
			{
				continue;
			}

			// Do not use itself as the starting location
			auto otherPipeConnection = pipeConnection->GetConnection();

			outputConnector = inputConnector = otherPipeConnection;

			auto pipe = Cast<AFGBuildablePipeline>(otherPipeConnection->GetOwner());

			if (pipe)
			{
				if (firstConnector)
				{
					firstConnector = !firstConnector;

					initialThroughtputLimit = AEfficiencyCheckerLogic::getPipeSpeed(pipe);
				}
				else
				{
					initialThroughtputLimit = FMath::Min(AEfficiencyCheckerLogic::getPipeSpeed(pipe), initialThroughtputLimit);
				}
			}

			fluidItem = pipeConnection->GetFluidDescriptor();

			break;
		}

		if (fluidItem)
		{
			collectSettings.GetCurrentFilter().allowedFiltered = true;
			collectSettings.GetCurrentFilter().allowedItems.Add(fluidItem);

			// collectSettings.GetInjectedInput()[fluidItem];
			// collectSettings.GetRequiredOutput()[fluidItem];

			collectSettings.SetResourceForm(UFGItemDescriptor::GetForm(fluidItem));
		}
	}
	else if (resourceForm == EResourceForm::RF_SOLID)
	{
		auto anchorPoint = GetActorLocation() + FVector(0, 0, 100);

		EC_LOG_Display_Condition(
			*getTagName(),
			*getTagName(),
			TEXT("Anchor point: X = "),
			anchorPoint.X,
			TEXT(" / Y = "),
			anchorPoint.Y,
			TEXT(" / Z = "),
			anchorPoint.Z
			);

		AFGBuildableConveyorBelt* currentConveyor = nullptr;
		FVector currenNearestCoord;

		// TArray<AActor*> allBelts;
		//
		// if (IsInGameThread())
		// {
		//     UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFGBuildableConveyorBelt::StaticClass(), allBelts);
		// }

		// Find intersecting belt
		TArray<AActor*> actorsToIgnore;
		actorsToIgnore.Add(this);

		TArray<struct FHitResult> hits;

		UKismetSystemLibrary::SphereTraceMulti(
			GetWorld(),
			anchorPoint,
			// Look from half meter below
			anchorPoint,
			50,
			// to half meter above
			traceChannel,
			false,
			actorsToIgnore,
			EDrawDebugTrace::None,
			hits,
			true
			);

		FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);

		for (const auto& hit : hits)
		{
			auto conveyor = Cast<AFGBuildableConveyorBelt>(hit.GetActor());
			if (!conveyor)
			{
				continue;
			}

			FVector nearestCoord;
			FVector direction;
			conveyor->GetLocationAndDirectionAtOffset(conveyor->FindOffsetClosestToLocation(anchorPoint), nearestCoord, direction);

			if (FVector::PointsAreNear(nearestCoord, anchorPoint, 50) && (
				!currentConveyor || FVector::Distance(nearestCoord, anchorPoint) < FVector::Distance(currenNearestCoord, anchorPoint)
			))
			{
				currenNearestCoord = nearestCoord;

				auto connection0Location = conveyor->GetConnection0()->GetConnectorLocation();
				auto connection1Location = conveyor->GetConnection1()->GetConnectorLocation();

				if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
				{
					EC_LOG_Display(*getTagName(), TEXT("Found intersecting conveyor "), *conveyor->GetName());
					EC_LOG_Display(
						*getTagName(),
						TEXT("    connection 0: X = "),
						connection0Location.X,
						TEXT(" / Y = "),
						connection0Location.Y,
						TEXT(" / Z = "),
						connection0Location.Z
						);
					EC_LOG_Display(
						*getTagName(),
						TEXT("    connection 1: X = "),
						connection1Location.X,
						TEXT(" / Y = "),
						connection1Location.Y,
						TEXT(" / Z = "),
						connection1Location.Z
						);
					EC_LOG_Display(
						*getTagName(),
						TEXT("    nearest location: X = "),
						nearestCoord.X,
						TEXT(" / Y = "),
						nearestCoord.Y,
						TEXT(" / Z = "),
						nearestCoord.Z
						);
				}

				currentConveyor = conveyor;
				inputConnector = conveyor->GetConnection0();
				outputConnector = conveyor->GetConnection1();

				initialThroughtputLimit = conveyor->GetSpeed() / 2;
			}
		}

		// if (!currentConveyor)
		// {
		// 	for (auto conveyorActor : AEfficiencyCheckerLogic::singleton->allBelts)
		// 	{
		// 		if (!IsValid(conveyorActor))
		// 		{
		// 			continue;
		// 		}
		//
		// 		auto conveyor = Cast<AFGBuildableConveyorBelt>(conveyorActor);
		//
		// 		if ((currentConveyor && conveyor->CreationTime < currentConveyor->CreationTime))
		// 		{
		// 			EC_LOG_Display_Condition(*getTagName(), TEXT("Conveyor "), *conveyor->GetName(), anchorPoint.X, TEXT(" was skipped"));
		//
		// 			continue;
		// 		}
		//
		// 		FVector nearestCoord;
		// 		FVector direction;
		// 		conveyor->GetLocationAndDirectionAtOffset(conveyor->FindOffsetClosestToLocation(anchorPoint), nearestCoord, direction);
		//
		// 		if (FVector::PointsAreNear(nearestCoord, anchorPoint, 50))
		// 		{
		// 			auto connection0Location = conveyor->GetConnection0()->GetConnectorLocation();
		// 			auto connection1Location = conveyor->GetConnection1()->GetConnectorLocation();
		//
		// 			if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		// 			{
		// 				EC_LOG_Display(*getTagName(), TEXT("Found intersecting conveyor "), *conveyor->GetName());
		// 				EC_LOG_Display(
		// 					*getTagName(),
		// 					TEXT("    connection 0: X = "),
		// 					connection0Location.X,
		// 					TEXT(" / Y = "),
		// 					connection0Location.Y,
		// 					TEXT(" / Z = "),
		// 					connection0Location.Z
		// 					);
		// 				EC_LOG_Display(
		// 					*getTagName(),
		// 					TEXT("    connection 1: X = "),
		// 					connection1Location.X,
		// 					TEXT(" / Y = "),
		// 					connection1Location.Y,
		// 					TEXT(" / Z = "),
		// 					connection1Location.Z
		// 					);
		// 				EC_LOG_Display(
		// 					*getTagName(),
		// 					TEXT("    nearest location: X = "),
		// 					nearestCoord.X,
		// 					TEXT(" / Y = "),
		// 					nearestCoord.Y,
		// 					TEXT(" / Z = "),
		// 					nearestCoord.Z
		// 					);
		// 			}
		//
		// 			currentConveyor = conveyor;
		// 			inputConnector = conveyor->GetConnection0();
		// 			outputConnector = conveyor->GetConnection1();
		//
		// 			initialThroughtputLimit = conveyor->GetSpeed() / 2;
		// 		}
		// 	}
		// }

		if (inputConnector || outputConnector)
		{
			TArray<TSubclassOf<UFGItemDescriptor>> allItems;
			UFGBlueprintFunctionLibrary::Cheat_GetAllDescriptors(allItems);

			for (auto item : allItems)
			{
				if (!item ||
					!UFGBlueprintFunctionLibrary::CanBeOnConveyor(item) ||
					UFGItemDescriptor::GetForm(item) != EResourceForm::RF_SOLID ||
					commonInfoSubsystem->wildCardItemDescriptors.Contains(item) ||
					commonInfoSubsystem->overflowItemDescriptors.Contains(item) ||
					commonInfoSubsystem->noneItemDescriptors.Contains(item) ||
					commonInfoSubsystem->anyUndefinedItemDescriptors.Contains(item)
					)
				{
					continue;;
				}

				collectSettings.GetCurrentFilter().allowedFiltered = true;
				collectSettings.GetCurrentFilter().allowedItems.Add(item);
			}
		}
	}
	else if ((resourceForm == EResourceForm::RF_LIQUID || resourceForm == EResourceForm::RF_GAS) && placementType == EPlacementType::PT_WALL)
	{
		auto anchorPoint = GetActorLocation();

		EC_LOG_Display_Condition(*getTagName(), TEXT("Anchor point: X = "), anchorPoint.X, TEXT(" / Y = "), anchorPoint.Y, TEXT(" / Z = "), anchorPoint.Z);

		AFGBuildablePipeline* currentPipe = nullptr;
		FVector currenNearestCoord;

		// Find intersecting pipe
		TArray<AActor*> actorsToIgnore;
		actorsToIgnore.Add(this);

		TArray<struct FHitResult> hits;

		UKismetSystemLibrary::SphereTraceMulti(
			GetWorld(),
			anchorPoint,
			anchorPoint,
			50,
			traceChannel,
			false,
			actorsToIgnore,
			EDrawDebugTrace::None,
			hits,
			true
			);

		FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);

		for (const auto& hit : hits)
		{
			auto pipe = Cast<AFGBuildablePipeline>(hit.GetActor());
			if (!pipe)
			{
				continue;
			}

			auto connection0Location = pipe->GetPipeConnection0()->GetConnectorLocation();
			auto connection1Location = pipe->GetPipeConnection1()->GetConnectorLocation();

			auto atConnection = FVector::PointsAreNear(connection0Location, anchorPoint, 1) ||
				FVector::PointsAreNear(connection1Location, anchorPoint, 1);

			auto closestLocation = false;

			if (!atConnection)
			{
				// Check if the pipe pass throught, instead of connecting with the wall checker
				FVector nearestCoord;
				FVector direction;
				pipe->GetLocationAndDirectionAtOffset(pipe->FindOffsetClosestToLocation(anchorPoint), nearestCoord, direction);

				closestLocation = FVector::PointsAreNear(nearestCoord, anchorPoint, 50) && (
					!currentPipe || FVector::Distance(nearestCoord, anchorPoint) < FVector::Distance(currenNearestCoord, anchorPoint)
				);

				if (closestLocation)
				{
					currenNearestCoord = nearestCoord;
				}
			}

			if (atConnection || closestLocation)
			{
				if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
				{
					EC_LOG_Display(*getTagName(), TEXT("Found connected pipe "), *pipe->GetName());
					EC_LOG_Display(
						*getTagName(),
						TEXT("    connection 0: X = "),
						connection0Location.X,
						TEXT(" / Y = "),
						connection0Location.Y,
						TEXT(" / Z = "),
						connection0Location.Z
						);
					EC_LOG_Display(
						*getTagName(),
						TEXT("    connection 1: X = "),
						connection1Location.X,
						TEXT(" / Y = "),
						connection1Location.Y,
						TEXT(" / Z = "),
						connection1Location.Z
						);
				}

				currentPipe = pipe;
				if (pipe->GetPipeConnection0()->IsConnected())
				{
					outputConnector = inputConnector = pipe->GetPipeConnection0();
				}
				else if (pipe->GetPipeConnection1()->IsConnected())
				{
					outputConnector = inputConnector = pipe->GetPipeConnection1();
				}

				TSubclassOf<UFGItemDescriptor> fluidItem = pipe->GetPipeConnection0()->GetFluidDescriptor();
				if (!fluidItem)
				{
					fluidItem = pipe->GetPipeConnection1()->GetFluidDescriptor();
				}

				if (fluidItem)
				{
					collectSettings.GetCurrentFilter().allowedFiltered = true;
					collectSettings.GetCurrentFilter().allowedItems.Add(fluidItem);

					// collectSettings.GetInjectedInput()[fluidItem];
					// collectSettings.GetRequiredOutput()[fluidItem];

					collectSettings.SetResourceForm(UFGItemDescriptor::GetForm(fluidItem));
				}

				initialThroughtputLimit = AEfficiencyCheckerLogic::getPipeSpeed(pipe);

				if (atConnection)
				{
					break;
				}
			}
		}

		// if (!currentPipe) {
		// 	for (auto pipeActor : AEfficiencyCheckerLogic::singleton->allPipes)
		// 	{
		// 		if (!IsValid(pipeActor))
		// 		{
		// 			continue;
		// 		}
		//
		// 		auto pipe = Cast<AFGBuildablePipeline>(pipeActor);
		//
		// 		if (currentPipe && pipe->CreationTime < currentPipe->CreationTime)
		// 		{
		// 			EC_LOG_Display_Condition(*getTagName(), TEXT("Pipe "), *pipe->GetName(), anchorPoint.X, TEXT(" was skipped"));
		//
		// 			continue;
		// 		}
		//
		// 		auto connection0Location = pipe->GetPipeConnection0()->GetConnectorLocation();
		// 		auto connection1Location = pipe->GetPipeConnection1()->GetConnectorLocation();
		//
		// 		if (FVector::PointsAreNear(connection0Location, anchorPoint, 1) ||
		// 			FVector::PointsAreNear(connection1Location, anchorPoint, 1))
		// 		{
		// 			if (IS_EC_LOG_LEVEL(ELogVerbosity::Log))
		// 			{
		// 				EC_LOG_Display(*getTagName(), TEXT("Found connected pipe "), *pipe->GetName());
		// 				EC_LOG_Display(
		// 					*getTagName(),
		// 					TEXT("    connection 0: X = "),
		// 					connection0Location.X,
		// 					TEXT(" / Y = "),
		// 					connection0Location.Y,
		// 					TEXT(" / Z = "),
		// 					connection0Location.Z
		// 					);
		// 				EC_LOG_Display(
		// 					*getTagName(),
		// 					TEXT("    connection 1: X = "),
		// 					connection1Location.X,
		// 					TEXT(" / Y = "),
		// 					connection1Location.Y,
		// 					TEXT(" / Z = "),
		// 					connection1Location.Z
		// 					);
		// 			}
		//
		// 			currentPipe = pipe;
		// 			if (pipe->GetPipeConnection0()->IsConnected())
		// 			{
		// 				inputConnector = pipe->GetPipeConnection0();
		// 				outputConnector = pipe->GetPipeConnection0();
		// 			}
		// 			else if (pipe->GetPipeConnection1()->IsConnected())
		// 			{
		// 				inputConnector = pipe->GetPipeConnection1();
		// 				outputConnector = pipe->GetPipeConnection1();
		// 			}
		//
		// 			TSubclassOf<UFGItemDescriptor> fluidItem = pipe->GetPipeConnection0()->GetFluidDescriptor();
		// 			if (! fluidItem)
		// 			{
		// 				fluidItem = pipe->GetPipeConnection1()->GetFluidDescriptor();
		// 			}
		//
		// 			if (fluidItem)
		// 			{
		// 				collectSettings.GetCurrentFilter().allowedFiltered = true;
		// 				collectSettings.GetCurrentFilter().allowedItems.Add(fluidItem);
		//
		// 				collectSettings.GetInjectedInput()[fluidItem];
		// 				collectSettings.GetRequiredOutput()[fluidItem];
		//
		// 				// collectSettings.GetInjectedItems().Add(fluidItem);
		// 			}
		//
		// 			initialThroughtputLimit = AEfficiencyCheckerLogic::getPipeSpeed(pipe);
		//
		// 			break;
		// 		}
		// 	}
		// }
	}

	float limitedThroughputIn = customInjectedInput ? injectedInput : initialThroughtputLimit;
	float limitedThroughputOut = customRequiredOutput ? requiredOutput : initialThroughtputLimit;

	time_t t = time(NULL);
	// time_t timeout = t + (time_t)AEfficiencyCheckerConfiguration::configuration.updateTimeout;
	collectSettings.SetTimeout(t + (time_t)AEfficiencyCheckerConfiguration::configuration.updateTimeout);

	EC_LOG_Warning_Condition(
		FUNCTIONSTR TEXT(": time = "),
		t,
		TEXT(" / timeout = "),
		collectSettings.GetTimeout(),
		TEXT(" / updateTimeout = "),
		AEfficiencyCheckerConfiguration::configuration.updateTimeout
		);

	if (AEfficiencyCheckerConfiguration::configuration.logicVersion >= 2)
	{
		if (inputConnector)
		{
			collectSettings.SetConnector(inputConnector);
			collectSettings.SetLimitedThroughput(limitedThroughputIn);

			AEfficiencyCheckerLogic2::collectInput(commonInfoSubsystem, collectSettings);

			limitedThroughputIn = collectSettings.GetLimitedThroughput();
			out_injectedInput = collectSettings.GetInjectedInputTotal();
		}

		if (outputConnector && !collectSettings.GetOverflow())
		{
			collectSettings.GetSeenActors().clear();

			collectSettings.SetConnector(outputConnector);
			collectSettings.SetLimitedThroughput(limitedThroughputOut);

			AEfficiencyCheckerLogic2::collectOutput(commonInfoSubsystem, collectSettings);

			limitedThroughputOut = collectSettings.GetLimitedThroughput();
			out_requiredOutput = collectSettings.GetRequiredOutputTotal();

			if (collectSettings.GetInjectedInput().empty())
			{
				for (auto entry : collectSettings.GetRequiredOutput())
				{
					// collectSettings.GetInjectedItems().Add(entry.first);
					collectSettings.GetInjectedInput()[entry.first];
				}
			}
		}

		for (const auto& entry : collectSettings.GetInjectedInput())
		{
			out_injectedItems.Add(entry.first);
		}
	}
	else
	{
		if (inputConnector)
		{
			collectSettings.SetConnector(inputConnector);
			collectSettings.SetLimitedThroughput(limitedThroughputIn);

			AEfficiencyCheckerLogic::singleton->collectInput(
				commonInfoSubsystem,
				collectSettings.GetResourceForm(),
				collectSettings.GetCustomInjectedInput(),
				inputConnector,
				out_injectedInput,
				collectSettings.GetLimitedThroughput(),
				collectSettings.GetSeenActors(),
				collectSettings.GetConnected(),
				out_injectedItems,
				collectSettings.GetCurrentFilter().allowedItems,
				collectSettings.GetBuildableSubsystem(),
				collectSettings.GetLevel(),
				collectSettings.GetOverflow(),
				collectSettings.GetIndent(),
				collectSettings.GetTimeout(),
				collectSettings.GetMachineStatusIncludeType()
				);

			limitedThroughputIn = collectSettings.GetLimitedThroughput();

			if (injectedItems.Num())
			{
				collectSettings.GetInjectedInput()[*injectedItems.begin()] = out_injectedInput;
			}
		}

		if (outputConnector && !customRequiredOutput && !collectSettings.GetOverflow())
		{
			collectSettings.GetSeenActors().clear();

			collectSettings.SetConnector(outputConnector);
			collectSettings.SetLimitedThroughput(limitedThroughputOut);

			AEfficiencyCheckerLogic::singleton->collectOutput(
				commonInfoSubsystem,
				collectSettings.GetResourceForm(),
				collectSettings.GetConnector(),
				out_requiredOutput,
				collectSettings.GetLimitedThroughput(),
				collectSettings.GetSeenActors(),
				collectSettings.GetConnected(),
				out_injectedItems,
				collectSettings.GetBuildableSubsystem(),
				collectSettings.GetLevel(),
				collectSettings.GetOverflow(),
				collectSettings.GetIndent(),
				collectSettings.GetTimeout(),
				collectSettings.GetMachineStatusIncludeType()
				);

			limitedThroughputOut = collectSettings.GetLimitedThroughput();

			if (injectedItems.Num())
			{
				collectSettings.GetRequiredOutput()[*injectedItems.begin()] = out_requiredOutput;
			}
		}
	}

	if (IS_EC_LOG_LEVEL(ELogVerbosity::Log) && !inputConnector && !outputConnector)
	{
		if (resourceForm == EResourceForm::RF_SOLID)
		{
			EC_LOG_Display(*getTagName(), TEXT("GetConnectedProduction: no intersecting belt"));
		}
		else
		{
			EC_LOG_Display(*getTagName(), TEXT("GetConnectedProduction: no intersecting pipe"));
		}
	}

	out_limitedThroughput = FMath::Min(limitedThroughputIn, limitedThroughputOut);

	EC_LOG_Display_Condition(TEXT("===="));
}

void AEfficiencyCheckerBuilding::UpdateConnectedProduction
(
	const bool keepCustomInput,
	const bool hasCustomInjectedInput,
	float in_customInjectedInput,
	const bool keepCustomOutput,
	const bool hasCustomRequiredOutput,
	float in_customRequiredOutput,
	bool includeProductionDetails
)
{
	if (HasAuthority())
	{
		Server_UpdateConnectedProduction(
			keepCustomInput,
			hasCustomInjectedInput,
			in_customInjectedInput,
			keepCustomOutput,
			hasCustomRequiredOutput,
			in_customRequiredOutput,
			includeProductionDetails
			);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling UpdateConnectedProduction at server"));

			rco->UpdateConnectedProductionRPC(
				this,
				keepCustomInput,
				hasCustomInjectedInput,
				in_customInjectedInput,
				keepCustomOutput,
				hasCustomRequiredOutput,
				in_customRequiredOutput,
				includeProductionDetails
				);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_UpdateConnectedProduction
(
	const bool keepCustomInput,
	const bool hasCustomInjectedInput,
	float in_customInjectedInput,
	const bool keepCustomOutput,
	const bool hasCustomRequiredOutput,
	float in_customRequiredOutput,
	bool includeProductionDetails
)
{
	if (!IsInGameThread())
	{
		AsyncTask(
			ENamedThreads::GameThread,
			[=, this]()
			{
				Server_UpdateConnectedProduction(
					keepCustomInput,
					hasCustomInjectedInput,
					in_customInjectedInput,
					keepCustomOutput,
					hasCustomRequiredOutput,
					in_customRequiredOutput,
					includeProductionDetails
					);
			}
			);

		return;
	}

	if (HasAuthority())
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Server_UpdateConnectedProduction"));

		limitedThroughput = 0;

		const TSet<AFGBuildable*> connectionsToUnbind(connectedBuildables);

		connectedBuildables.Empty();

		if (!keepCustomInput)
		{
			customInjectedInput = hasCustomInjectedInput;

			if (customInjectedInput)
			{
				injectedInput = in_customInjectedInput;
			}
		}

		if (! customInjectedInput)
		{
			injectedInput = 0;
		}

		if (!keepCustomOutput)
		{
			customRequiredOutput = hasCustomRequiredOutput;

			if (customRequiredOutput)
			{
				requiredOutput = in_customRequiredOutput;
			}
		}

		if (!customRequiredOutput)
		{
			requiredOutput = 0;
		}

		TSet<TSubclassOf<UFGItemDescriptor>> injectedItemsSet;

		overflow = false;

		TArray<FProductionDetail> producers;
		TArray<FProductionDetail> consumers;

		float tempInjectedInput = 0;
		float tempRequiredOutput = 0;

		GetConnectedProduction(
			tempInjectedInput,
			limitedThroughput,
			tempRequiredOutput,
			injectedItemsSet,
			connectedBuildables,
			producers,
			consumers,
			overflow,
			includeProductionDetails
			);

		if (!customInjectedInput)
		{
			injectedInput = tempInjectedInput;
		}
		if (!customRequiredOutput)
		{
			requiredOutput = tempRequiredOutput;
		}

		injectedItems = injectedItemsSet.Array();

		lastUpdated = GetWorld()->GetTimeSeconds();
		updateRequested = 0;

		SetActorTickEnabled(false);
		//mFactoryTickFunction.SetTickFunctionEnable(false);

		if ((AEfficiencyCheckerConfiguration::configuration.autoUpdate && autoUpdateMode == EAutoUpdateType::AUT_USE_DEFAULT) ||
			autoUpdateMode == EAutoUpdateType::AUT_ENABLED)
		{
			// Remove bindings for all that are on connectionsToUnbind but not on connectedBuildables
			const auto bindingsToRemove = connectionsToUnbind.Difference(connectedBuildables);
			removeOnDestroyBindings(bindingsToRemove);
			removeOnRecipeChangedBindings(bindingsToRemove);
			removeOnSortRulesChangedDelegateBindings(bindingsToRemove);

			// Add bindings for all that are on connectedBuildables but not on connectionsToUnbind
			const auto bindingsToAdd = connectedBuildables.Difference(connectionsToUnbind);
			addOnDestroyBindings(bindingsToAdd);
			addOnRecipeChangedBindings(bindingsToAdd);
			addOnSortRulesChangedDelegateBindings(bindingsToAdd);
		}

		UpdateItem(injectedInput, limitedThroughput, requiredOutput, injectedItems, producers, consumers, overflow);
	}
	else
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Server_UpdateConnectedProduction - no authority"));
	}

	EC_LOG_Display_Condition(TEXT("===="));
}

void AEfficiencyCheckerBuilding::addOnDestroyBindings(const TSet<AFGBuildable*>& buildings)
{
	for (auto building : buildings)
	{
		AddOnDestroyBinding(building);
	}
}

void AEfficiencyCheckerBuilding::removeOnDestroyBindings(const TSet<AFGBuildable*>& buildings)
{
	for (auto building : buildings)
	{
		RemoveOnDestroyBinding(building);
	}
}

void AEfficiencyCheckerBuilding::addOnSortRulesChangedDelegateBindings(const TSet<AFGBuildable*>& buildings)
{
	for (auto building : buildings)
	{
		const auto smart = Cast<AFGBuildableSplitterSmart>(building);
		if (smart)
		{
			AddOnSortRulesChangedDelegateBinding(smart);
		}
	}
}

void AEfficiencyCheckerBuilding::removeOnSortRulesChangedDelegateBindings(const TSet<AFGBuildable*>& buildings)
{
	for (auto building : buildings)
	{
		const auto smart = Cast<AFGBuildableSplitterSmart>(building);
		if (smart)
		{
			RemoveOnSortRulesChangedDelegateBinding(smart);
		}
	}
}

void AEfficiencyCheckerBuilding::addOnRecipeChangedBindings(const TSet<AFGBuildable*>& buildings)
{
	for (auto building : buildings)
	{
		const auto manufacturer = Cast<AFGBuildableManufacturer>(building);
		if (manufacturer)
		{
			AddOnRecipeChangedBinding(manufacturer);
		}
	}
}

void AEfficiencyCheckerBuilding::removeOnRecipeChangedBindings(const TSet<AFGBuildable*>& buildings)
{
	for (auto building : buildings)
	{
		const auto manufacturer = Cast<AFGBuildableManufacturer>(building);
		if (manufacturer)
		{
			RemoveOnRecipeChangedBinding(manufacturer);
		}
	}
}

void AEfficiencyCheckerBuilding::RemoveBuilding(AFGBuildable* buildable)
{
	if (HasAuthority())
	{
		Server_RemoveBuilding(buildable);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling RemoveBuilding at server"));

			rco->RemoveBuildingRPC(this, buildable);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_RemoveBuilding(AFGBuildable* buildable)
{
	if (HasAuthority())
	{
		pendingBuildables.Remove(buildable);
		connectedBuildables.Remove(buildable);
	}
}

void AEfficiencyCheckerBuilding::setPendingPotentialCallback(class AFGBuildableFactory* buildable, float potential)
{
	if (!AEfficiencyCheckerLogic::singleton)
	{
		return;
	}

	EC_LOG_Display_Condition(
		TEXT("SetPendingPotential of building "),
		*GetPathNameSafe(buildable),
		TEXT(" to "),
		potential
		);

	// Update all EfficiencyCheckerBuildings that connects to this building
	FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);

	for (auto efficiencyBuilding : AEfficiencyCheckerLogic::singleton->allEfficiencyBuildings)
	{
		if (efficiencyBuilding->HasAuthority() && efficiencyBuilding->connectedBuildables.Contains(buildable))
		{
			efficiencyBuilding->Server_UpdateConnectedProduction(true, false, 0, true, false, 0);
		}
	}
}

void AEfficiencyCheckerBuilding::setPendingProductionBoostCallback(class AFGBuildableFactory* buildable, float productionBoost)
{
	if (!AEfficiencyCheckerLogic::singleton)
	{
		return;
	}

	EC_LOG_Display_Condition(
		TEXT("setPendingProductionBoost of building "),
		*GetPathNameSafe(buildable),
		TEXT(" to "),
		productionBoost
		);

	// Update all EfficiencyCheckerBuildings that connects to this building
	FScopeLock ScopeLock(&ACommonInfoSubsystem::mclCritical);

	for (auto efficiencyBuilding : AEfficiencyCheckerLogic::singleton->allEfficiencyBuildings)
	{
		if (efficiencyBuilding->HasAuthority() && efficiencyBuilding->connectedBuildables.Contains(buildable))
		{
			efficiencyBuilding->Server_UpdateConnectedProduction(true, false, 0, true, false, 0);
		}
	}
}

void AEfficiencyCheckerBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, injectedItems);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, injectedInput);
	DOREPLIFETIME(AEfficiencyCheckerBuilding, customInjectedInput);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, limitedThroughput);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, requiredOutput);
	DOREPLIFETIME(AEfficiencyCheckerBuilding, customRequiredOutput);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, overflow);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, autoUpdateMode);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, machineStatusIncludeType);

	DOREPLIFETIME(AEfficiencyCheckerBuilding, innerPipelineAttachment);

	// DOREPLIFETIME(AEfficiencyCheckerBuilding, connectedBuildables);

	// DOREPLIFETIME(AEfficiencyCheckerBuilding, pendingBuildables);
}

void AEfficiencyCheckerBuilding::AddPendingBuilding(AFGBuildable* buildable)
{
	if (HasAuthority())
	{
		Server_AddPendingBuilding(buildable);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling AddPendingBuilding at server"));

			rco->AddPendingBuildingRPC(this, buildable);
		}
	}
}

void AEfficiencyCheckerBuilding::Server_AddPendingBuilding(AFGBuildable* buildable)
{
	if (HasAuthority())
	{
		pendingBuildables.Add(buildable);
	}
}

void AEfficiencyCheckerBuilding::UpdateItem_Implementation
(
	float in_injectedInput,
	float in_limitedThroughput,
	float in_requiredOutput,
	const TArray<TSubclassOf<UFGItemDescriptor>>& in_injectedItems,
	const TArray<FProductionDetail>& in_producers,
	const TArray<FProductionDetail>& in_consumers,
	bool in_overflow
)
{
	OnUpdateItem.Broadcast(
		in_injectedInput,
		in_limitedThroughput,
		in_requiredOutput,
		in_injectedItems,
		in_producers,
		in_consumers,
		in_overflow
		);
}

#ifndef OPTIMIZE
#pragma optimize("", on)
#endif
