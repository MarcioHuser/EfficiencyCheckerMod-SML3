#pragma once
#include "CoreMinimal.h"
#include "Configuration/ConfigManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EfficiencyChecker_ConfigStruct.generated.h"

/* Struct generated from Mod Configuration Asset '/EfficiencyCheckerMod/Configuration/EfficiencyChecker_Config' */
USTRUCT(BlueprintType)
struct FEfficiencyChecker_ConfigStruct {
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadWrite)
    bool autoUpdate{};

    UPROPERTY(BlueprintReadWrite)
    float autoUpdateTimeout{};

    UPROPERTY(BlueprintReadWrite)
    float autoUpdateDistance{};

    UPROPERTY(BlueprintReadWrite)
    int32 logLevel{};

    UPROPERTY(BlueprintReadWrite)
    bool ignoreStorageTeleporter{};

    UPROPERTY(BlueprintReadWrite)
    float updateTimeout{};

    UPROPERTY(BlueprintReadWrite)
    int32 logicVersion{};

    /* Retrieves active configuration value and returns object of this struct containing it */
    static FEfficiencyChecker_ConfigStruct GetActiveConfig(UObject* WorldContext) {
        FEfficiencyChecker_ConfigStruct ConfigStruct{};
        FConfigId ConfigId{"EfficiencyCheckerMod", ""};
        if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)) {
            UConfigManager* ConfigManager = World->GetGameInstance()->GetSubsystem<UConfigManager>();
            ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{FEfficiencyChecker_ConfigStruct::StaticStruct(), &ConfigStruct});
        }
        return ConfigStruct;
    }
};

