﻿#include "EfficiencyCheckerEquipment.h"

#include "Buildables/FGBuildableConveyorBase.h"
#include "EfficiencyCheckerModModule.h"
#include "EfficiencyCheckerRCO.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPlayerController.h"
#include "Logic/EfficiencyCheckerLogic.h"
#include "Util/EfficiencyCheckerOptimize.h"
#include "Util/Logging.h"
#include "EfficiencyChecker_ConfigStruct.h"

#include <map>

#include "Buildables/FGBuildablePipeline.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

FString AEfficiencyCheckerEquipment::getAuthorityAndPlayer(const AActor* actor)
{
	return FString(TEXT("Has Authority = ")) +
		(actor->HasAuthority() ? TEXT("true") : TEXT("false")) +
		TEXT(" / Character = ") +
		(actor->GetInstigator() ? *actor->GetInstigator()->GetHumanReadableName() : TEXT("None"));
}

AEfficiencyCheckerEquipment::AEfficiencyCheckerEquipment()
{
}

void AEfficiencyCheckerEquipment::BeginPlay()
{
	Super::BeginPlay();
}

void AEfficiencyCheckerEquipment::PrimaryFirePressed(AFGBuildable* targetBuildable)
{
	EC_LOG_Display_Condition(
		*getTagName(),
		TEXT("PrimaryFirePressed = "),
		*GetPathName(),
		TEXT(" / Target = "),
		*GetPathNameSafe(targetBuildable),
		TEXT(" / "),
		*getAuthorityAndPlayer(this)
		);

	if (HasAuthority())
	{
		PrimaryFirePressed_Server(targetBuildable);
	}
	else
	{
		auto rco = UEfficiencyCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Calling PrimaryFirePressed at server"));

			rco->PrimaryFirePressedPC(this, targetBuildable);
		}
	}
}

void AEfficiencyCheckerEquipment::PrimaryFirePressed_Server(AFGBuildable* targetBuildable)
{
	if (!HasAuthority())
	{
		return;
	}

	float injectedInput = 0;
	float requiredOutput = 0;

	UFGConnectionComponent* inputConnector = nullptr;
	UFGConnectionComponent* outputConnector = nullptr;

	TSet<TSubclassOf<UFGItemDescriptor>> restrictedItems;

	auto resourceForm = EResourceForm::RF_INVALID;

	TSet<AFGBuildable*> connected;
	const auto buildableSubsystem = AFGBuildableSubsystem::Get(GetWorld());
	const FString indent(TEXT("    "));
	TSet<TSubclassOf<UFGItemDescriptor>> injectedItemsSet;

	float initialThroughtputLimit = 0;
	bool overflow = false;

	if (targetBuildable)
	{
		auto conveyor = Cast<AFGBuildableConveyorBase>(targetBuildable);
		if (conveyor)
		{
			inputConnector = conveyor->GetConnection0();
			outputConnector = conveyor->GetConnection1();

			initialThroughtputLimit = conveyor->GetSpeed() / 2;

			resourceForm = EResourceForm::RF_SOLID;

			TArray<TSubclassOf<UFGItemDescriptor>> allItems;
			UFGBlueprintFunctionLibrary::Cheat_GetAllDescriptors(allItems);

			for (auto item : allItems)
			{
				if (!item ||
					!UFGBlueprintFunctionLibrary::CanBeOnConveyor(item) ||
					UFGItemDescriptor::GetForm(item) != EResourceForm::RF_SOLID ||
					AEfficiencyCheckerLogic::singleton->wildCardItemDescriptors.Contains(item) ||
					AEfficiencyCheckerLogic::singleton->overflowItemDescriptors.Contains(item) ||
					AEfficiencyCheckerLogic::singleton->noneItemDescriptors.Contains(item) ||
					AEfficiencyCheckerLogic::singleton->anyUndefinedItemDescriptors.Contains(item)
					)
				{
					continue;;
				}

				restrictedItems.Add(item);
			}
		}
		else
		{
			auto pipe = Cast<AFGBuildablePipeline>(targetBuildable);
			if (pipe)
			{
				if (pipe->GetPipeConnection0()->IsConnected())
				{
					inputConnector = pipe->GetPipeConnection0();
					outputConnector = pipe->GetPipeConnection0();
				}
				else if (pipe->GetPipeConnection1()->IsConnected())
				{
					inputConnector = pipe->GetPipeConnection1();
					outputConnector = pipe->GetPipeConnection1();
				}

				initialThroughtputLimit = AEfficiencyCheckerLogic::getPipeSpeed(pipe);

				TSubclassOf<UFGItemDescriptor> fluidItem = pipe->GetPipeConnection0()->GetFluidDescriptor();
				if (!fluidItem)
				{
					fluidItem = pipe->GetPipeConnection1()->GetFluidDescriptor();
				}

				if (fluidItem)
				{
					restrictedItems.Add(fluidItem);
					injectedItemsSet.Add(fluidItem);

					resourceForm = UFGItemDescriptor::GetForm(fluidItem);
				}
			}
			else
			{
				return;
			}
		}
	}

	float limitedThroughputIn = initialThroughtputLimit;

	time_t t = time(NULL);
	time_t timeout = t + (time_t)AEfficiencyCheckerLogic::configuration.updateTimeout;

	EC_LOG_Warning_Condition(
		__FUNCTION__ TEXT(": time = "),
		t,
		TEXT(" / timeout = "),
		timeout,
		TEXT(" / updateTimeout = "),
		AEfficiencyCheckerLogic::configuration.updateTimeout
		);

	if (inputConnector)
	{
		TSet<AActor*> seenActors;

		AEfficiencyCheckerLogic::collectInput(
			resourceForm,
			false,
			inputConnector,
			injectedInput,
			limitedThroughputIn,
			seenActors,
			connected,
			injectedItemsSet,
			restrictedItems,
			buildableSubsystem,
			0,
			overflow,
			indent,
			timeout,
			machineStatusIncludeType
			);
	}

	float limitedThroughputOut = initialThroughtputLimit;

	if (outputConnector && !overflow)
	{
		std::map<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>> seenActors;

		AEfficiencyCheckerLogic::collectOutput(
			resourceForm,
			outputConnector,
			requiredOutput,
			limitedThroughputOut,
			seenActors,
			connected,
			injectedItemsSet,
			buildableSubsystem,
			0,
			overflow,
			indent,
			timeout,
			machineStatusIncludeType
			);
	}

	if (inputConnector || outputConnector)
	{
		ShowStatsWidget(injectedInput, FMath::Min(limitedThroughputIn, limitedThroughputOut), requiredOutput, injectedItemsSet.Array(), overflow);
	}
}

void AEfficiencyCheckerEquipment::ShowStatsWidget_Implementation
(
	float in_injectedInput,
	float in_limitedThroughput,
	float in_requiredOutput,
	const TArray<TSubclassOf<UFGItemDescriptor>>& in_injectedItems,
	bool in_overflow
)
{
	EC_LOG_Display_Condition(
		*getTagName(),
		TEXT("Broadcasting ShowStats = "),
		*GetPathName()
		);

	OnShowStatsWidget.Broadcast(in_injectedInput, in_limitedThroughput, in_requiredOutput, in_injectedItems, in_overflow);
}

#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
