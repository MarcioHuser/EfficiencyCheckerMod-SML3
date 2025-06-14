#include "EfficiencyCheckerRCO.h"

#include "Logic/EfficiencyCheckerLogic.h"
#include "EfficiencyCheckerEquipment.h"
#include "EfficiencyCheckerBuilding.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Util/ECMOptimize.h"
#include "Util/ECMLogging.h"

#ifndef OPTIMIZE
#pragma optimize("", off )
#endif

void UEfficiencyCheckerRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEfficiencyCheckerRCO, dummy);
}

UEfficiencyCheckerRCO* UEfficiencyCheckerRCO::getRCO(UWorld* world)
{
	auto rco = Cast<UEfficiencyCheckerRCO>(
		Cast<AFGPlayerController>(world->GetFirstPlayerController())->GetRemoteCallObjectOfClass(UEfficiencyCheckerRCO::StaticClass())
		);

	return rco;
}

void UEfficiencyCheckerRCO::UpdateBuildingRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* newBuildable)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->Server_UpdateBuilding(newBuildable);
	}
}

bool UEfficiencyCheckerRCO::UpdateBuildingRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* newBuildable)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::UpdateConnectedProductionRPC_Implementation
(
	AEfficiencyCheckerBuilding* efficiencyChecker,
	bool keepCustomInput,
	bool hasCustomInjectedInput,
	float in_customInjectedInput,
	bool keepCustomOutput,
	bool hasCustomRequiredOutput,
	float in_customRequiredOutput,
	bool includeProductionDetails
)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->Server_UpdateConnectedProduction(
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

bool UEfficiencyCheckerRCO::UpdateConnectedProductionRPC_Validate
(
	AEfficiencyCheckerBuilding* efficiencyChecker,
	bool keepCustomInput,
	bool hasCustomInjectedInput,
	float in_customInjectedInput,
	bool keepCustomOutput,
	bool hasCustomRequiredOutput,
	float in_customRequiredOutput,
	bool includeProductionDetails
)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::RemoveBuildingRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->Server_RemoveBuilding(buildable);
	}
}

bool UEfficiencyCheckerRCO::RemoveBuildingRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::AddPendingBuildingRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->Server_AddPendingBuilding(buildable);
	}
}

bool UEfficiencyCheckerRCO::AddPendingBuildingRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::SetCustomInjectedInputRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->SetCustomInjectedInput(enabled, value);
	}
}

bool UEfficiencyCheckerRCO::SetCustomInjectedInputRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::SetCustomRequiredOutputRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->SetCustomRequiredOutput(enabled, value);
	}
}

bool UEfficiencyCheckerRCO::SetCustomRequiredOutputRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::SetAutoUpdateModeRPC_Implementation(class AEfficiencyCheckerBuilding* efficiencyChecker, EAutoUpdateType autoUpdateMode)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->SetAutoUpdateMode(autoUpdateMode);
	}
}

bool UEfficiencyCheckerRCO::SetAutoUpdateModeRPC_Validate(class AEfficiencyCheckerBuilding* efficiencyChecker, EAutoUpdateType autoUpdateMode)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::SetMachineStatusIncludeTypeRPC_Implementation(class AEfficiencyCheckerBuilding* efficiencyChecker, int32 machineStatusIncludeType)
{
	if (efficiencyChecker && efficiencyChecker->HasAuthority())
	{
		efficiencyChecker->SetMachineStatusIncludeType(machineStatusIncludeType);
	}
}

bool UEfficiencyCheckerRCO::SetMachineStatusIncludeTypeRPC_Validate(class AEfficiencyCheckerBuilding* efficiencyChecker, int32 machineStatusIncludeType)
{
	return !!efficiencyChecker;
}

void UEfficiencyCheckerRCO::PrimaryFirePressedPC_Implementation(class AEfficiencyCheckerEquipment* efficiencyCheckerEquip, AFGBuildable* targetBuildable)
{
	if (efficiencyCheckerEquip && efficiencyCheckerEquip->HasAuthority())
	{
		efficiencyCheckerEquip->PrimaryFirePressed_Server(targetBuildable);
	}
}

bool UEfficiencyCheckerRCO::PrimaryFirePressedPC_Validate(class AEfficiencyCheckerEquipment* efficiencyCheckerEquip, AFGBuildable* targetBuildable)
{
	return !!efficiencyCheckerEquip;
}

#ifndef OPTIMIZE
#pragma optimize("", on)
#endif
