#include "Util/EfficiencyCheckerConfiguration.h"
#include "Util/EfficiencyCheckerOptimize.h"
#include "EfficiencyCheckerBuilding.h"
#include "EfficiencyChecker_ConfigStruct.h"
#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Patching/NativeHookManager.h"
#include "Util/Logging.h"

FEfficiencyChecker_ConfigStruct AEfficiencyCheckerConfiguration::configuration;

void AEfficiencyCheckerConfiguration::SetEfficiencyCheckerConfiguration(const FEfficiencyChecker_ConfigStruct& in_configuration)
{
	configuration = in_configuration;

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	EC_LOG_Display(TEXT("setConfiguration"));

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
	EC_LOG_Display(TEXT("logicVersion = "), configuration.logicVersion);

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

void AEfficiencyCheckerConfiguration::GetEfficiencyCheckerConfiguration(bool& out_autoUpdate, int& out_logLevel, float& out_autoUpdateTimeout, float& out_autoUpdateDistance)
{
	EC_LOG_Display_Condition(TEXT("AEfficiencyCheckerConfiguration: GetEfficiencyCheckerSettings"));

	out_autoUpdate = AEfficiencyCheckerConfiguration::configuration.autoUpdate;
	out_logLevel = AEfficiencyCheckerConfiguration::configuration.logLevel;
	out_autoUpdateTimeout = AEfficiencyCheckerConfiguration::configuration.autoUpdateTimeout;
	out_autoUpdateDistance = AEfficiencyCheckerConfiguration::configuration.autoUpdateDistance;
}

bool AEfficiencyCheckerConfiguration::IsAutoUpdateEnabled()
{
	return configuration.autoUpdate;
}

int AEfficiencyCheckerConfiguration::GetLogLevelECM()
{
	return configuration.logLevel;
}

float AEfficiencyCheckerConfiguration::GetAutoUpdateTimeout()
{
	return configuration.autoUpdateTimeout;
}

float AEfficiencyCheckerConfiguration::GetAutoUpdateDistance()
{
	return configuration.autoUpdateDistance;
}
