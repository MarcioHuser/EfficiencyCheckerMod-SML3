// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo

#include "EfficiencyCheckerModModule.h"
#include "Util/ECMOptimize.h"

void FEfficiencyCheckerModModule::StartupModule()
{
}

void FEfficiencyCheckerModModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FEfficiencyCheckerModModule, EfficiencyCheckerMod)
