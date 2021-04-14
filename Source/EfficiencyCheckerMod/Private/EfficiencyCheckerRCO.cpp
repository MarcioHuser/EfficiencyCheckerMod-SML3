#include "EfficiencyCheckerRCO.h"
#include "EfficiencyCheckerBuilding.h"
#include "EFficiencyCheckerEquipment.h"
#include "Util/Optimize.h"

#include "FGPlayerController.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

void UEfficiencyCheckerRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UEfficiencyCheckerRCO, dummy);
}

UEfficiencyCheckerRCO* UEfficiencyCheckerRCO::getRCO(UWorld* world)
{
    return Cast<UEfficiencyCheckerRCO>(
        Cast<AFGPlayerController>(world->GetFirstPlayerController())->GetRemoteCallObjectOfClass(UEfficiencyCheckerRCO::StaticClass())
        );
}

void UEfficiencyCheckerRCO::UpdateBuildingRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* newBuildable)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->Server_UpdateBuilding(newBuildable);
    }
}

bool UEfficiencyCheckerRCO::UpdateBuildingRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* newBuildable)
{
    return true;
}

void UEfficiencyCheckerRCO::UpdateConnectedProductionRPC_Implementation
(
    AEfficiencyCheckerBuilding* efficiencyChecker,
    bool keepCustomInput,
    bool hasCustomInjectedInput,
    float in_customInjectedInput,
    bool keepCustomOutput,
    bool hasCustomRequiredOutput,
    float in_customRequiredOutput
)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->Server_UpdateConnectedProduction(
            keepCustomInput,
            hasCustomInjectedInput,
            in_customInjectedInput,
            keepCustomOutput,
            hasCustomRequiredOutput,
            in_customRequiredOutput
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
    float in_customRequiredOutput
)
{
    return true;
}

void UEfficiencyCheckerRCO::RemoveBuildingRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->Server_RemoveBuilding(buildable);
    }
}

bool UEfficiencyCheckerRCO::RemoveBuildingRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
    return true;
}

void UEfficiencyCheckerRCO::AddPendingBuildingRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->Server_AddPendingBuilding(buildable);
    }
}

bool UEfficiencyCheckerRCO::AddPendingBuildingRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable)
{
    return true;
}

void UEfficiencyCheckerRCO::SetCustomInjectedInputRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->SetCustomInjectedInput(enabled, value);
    }
}

bool UEfficiencyCheckerRCO::SetCustomInjectedInputRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
    return true;
}

void UEfficiencyCheckerRCO::SetCustomRequiredOutputRPC_Implementation(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->SetCustomRequiredOutput(enabled, value);
    }
}

bool UEfficiencyCheckerRCO::SetCustomRequiredOutputRPC_Validate(AEfficiencyCheckerBuilding* efficiencyChecker, bool enabled, float value)
{
    return true;
}

void UEfficiencyCheckerRCO::SetAutoUpdateModeRPC_Implementation(class AEfficiencyCheckerBuilding* efficiencyChecker, EAutoUpdateType autoUpdateMode)
{
    if (efficiencyChecker->HasAuthority())
    {
        efficiencyChecker->SetAutoUpdateMode(autoUpdateMode);
    }
}

bool UEfficiencyCheckerRCO::SetAutoUpdateModeRPC_Validate(class AEfficiencyCheckerBuilding* efficiencyChecker, EAutoUpdateType autoUpdateMode)
{
    return true;
}

void UEfficiencyCheckerRCO::PrimaryFirePressedPC_Implementation(class AEfficiencyCheckerEquipment* efficiencyCheckerEquip, AFGBuildable* targetBuildable)
{
    if(efficiencyCheckerEquip->HasAuthority())
    {
        efficiencyCheckerEquip->PrimaryFirePressed_Server(targetBuildable);
    }
}

bool UEfficiencyCheckerRCO::PrimaryFirePressedPC_Validate(class AEfficiencyCheckerEquipment* efficiencyCheckerEquip, AFGBuildable* targetBuildable)
{
    return true;
}

#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
