// ReSharper disable CppUPropertyMacroCallHasNoEffect
#pragma once

#include "FGRemoteCallObject.h"
#include "EfficiencyCheckerRCO.generated.h"

enum class EAutoUpdateType : uint8;

UCLASS()
class EFFICIENCYCHECKERMOD_API UEfficiencyCheckerRCO : public UFGRemoteCallObject
{
    GENERATED_BODY()
public:

    static UEfficiencyCheckerRCO* getRCO(UWorld* world);

    UFUNCTION(BlueprintCallable, Category="EfficiencyCheckerRCO", DisplayName="Get Efficiency Checker RCO")
    static UEfficiencyCheckerRCO*
    getRCO(AActor* actor)
    {
        return getRCO(actor->GetWorld());
    }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="UpdateBuilding")
    virtual void UpdateBuildingRPC(class AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* newBuildable);

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="UpdateConnectedProduction")
    virtual void UpdateConnectedProductionRPC
    (
        class AEfficiencyCheckerBuilding* efficiencyChecker,
        bool keepCustomInput,
        bool hasCustomInjectedInput,
        UPARAM(DisplayName = "Custom Injected Input") float in_customInjectedInput,
        bool keepCustomOutput,
        bool hasCustomRequiredOutput,
        UPARAM(DisplayName = "Custom Required Output") float in_customRequiredOutput
    );

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="RemoveBuilding")
    virtual void RemoveBuildingRPC(class AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable);

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="AddPendingBuilding")
    virtual void AddPendingBuildingRPC(class AEfficiencyCheckerBuilding* efficiencyChecker, AFGBuildable* buildable);

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="SetCustomInjectedInput")
    virtual void SetCustomInjectedInputRPC(class AEfficiencyCheckerBuilding* efficiencyChecker,  bool enabled, float value);

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="SetCustomRequiredOutput")
    virtual void SetCustomRequiredOutputRPC(class AEfficiencyCheckerBuilding* efficiencyChecker,  bool enabled, float value);

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="SetAutoUpdateMode")
    virtual void SetAutoUpdateModeRPC(class AEfficiencyCheckerBuilding* efficiencyChecker, EAutoUpdateType autoUpdateMode);

    UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="EfficiencyCheckerRCO",DisplayName="SetAutoUpdateMode")
    virtual void PrimaryFirePressedPC(class AEfficiencyCheckerEquipment* efficiencyCheckerEquip, AFGBuildable* targetBuildable);

    UPROPERTY(Replicated)
    bool dummy = true;
};
