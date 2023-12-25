// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo

#include "EfficiencyCheckerHologramWall.h"

#include "Buildables/FGBuildableWall.h"
#include "Components/WidgetComponent.h"
#include "EfficiencyCheckerBuilding.h"
#include "FGConstructDisqualifier.h"
#include "FGFactoryConnectionComponent.h"
#include "Util/ECMOptimize.h"
#include "Util/ECMLogging.h"

#ifndef OPTIMIZE
#pragma optimize("", off )
#endif

AEfficiencyCheckerHologramWall::AEfficiencyCheckerHologramWall()
	: Super()
{
	this->mValidHitClasses.Add(AFGBuildableWall::StaticClass());
	//this->mScrollMode = EHologramScrollMode::HSM_ROTATE;
}

// Called when the game starts or when spawned
void AEfficiencyCheckerHologramWall::BeginPlay()
{
	Super::BeginPlay();

	_TAG_NAME = GetName() + TEXT(": ");

	if (HasAuthority())
	{
		TInlineComponentArray<UWidgetComponent*> widgets;

		GetCheckerBuildable()->GetComponents(widgets);

		for (auto widget : widgets)
		{
			widget->SetVisibility(false);
		}
	}
}

bool AEfficiencyCheckerHologramWall::IsValidHitResult(const FHitResult& hitResult) const
{
	const auto defaultBuildable = GetCheckerBuildable();

	bool ret = false;

	auto wallCheck = Cast<AFGBuildableWall>(hitResult.GetActor());

	if (defaultBuildable->resourceForm == EResourceForm::RF_SOLID &&
		wallCheck &&
		wallCheck->GetComponentByClass(UFGFactoryConnectionComponent::StaticClass()))
	{
		ret = true;
	}

	static float lastCheck = 0;

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display_Condition(*getTagName(), TEXT("IsValidHitResult = "), ret);

		dumpDisqualifiers();

		if (hitResult.GetActor())
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
			//
			// if (wall)
			// {
			//     EC_LOG_Display_Condition(*getTagName(), TEXT("Is Wall"));
			//
			//     for (const auto component : wall->GetComponents())
			//     {
			//         EC_LOG_Display_Condition(*getTagName(), TEXT("    "), *component->GetName(), TEXT(" / "), *GetPathNameSafe(component->GetClass()));
			//     }
			// }
		}

		EC_LOG_Display_Condition(TEXT("===="));
	}

	return ret;
}

void AEfficiencyCheckerHologramWall::AdjustForGround(FVector& out_adjustedLocation, FRotator& out_adjustedRotation)
{
	static float lastCheck = 0;

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Before AdjustForGround"));

		EC_LOG_Display_Condition(*getTagName(), TEXT("Hologram = "), *GetName());

		FVector location = GetActorLocation();
		FRotator rotator = GetActorRotation();

		EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
		EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

		// EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
		//
		// location = hitResult.GetActor()->GetActorLocation();
		// rotator = hitResult.GetActor()->GetActorRotation();
		//
		// EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
		// EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
	}

	bool isSnapped = false;
	wall = nullptr;

	FVector nearestCoord;
	FVector direction;

	const auto defaultBuildable = GetCheckerBuildable();

	if (defaultBuildable->resourceForm == EResourceForm::RF_SOLID)
	{
		wall = Cast<AFGBuildableWall>(lastHit_.GetActor());

		if (wall)
		{
			nearestCoord = wall->GetActorLocation();
			direction = wall->GetActorRotation().Vector();

			TInlineComponentArray<UFGFactoryConnectionComponent*> components;
			wall->GetComponents(components);

			UFGFactoryConnectionComponent* nearestConnection = nullptr;

			for (const auto attachment : components)
			{
				if (FVector::Dist(attachment->GetComponentLocation(), lastHit_.Location) <= 300 &&
					(!nearestConnection || FVector::Dist(attachment->GetComponentLocation(), lastHit_.Location) < FVector::Dist(
						nearestConnection->GetComponentLocation(),
						lastHit_.Location
						)))
				{
					nearestConnection = attachment;
				}
			}

			if (nearestConnection)
			{
				out_adjustedRotation = nearestConnection->GetComponentRotation().Add(0, rotationDelta * 180, 0);
				out_adjustedLocation = FVector(nearestConnection->GetComponentLocation().X, nearestConnection->GetComponentLocation().Y, wall->GetActorLocation().Z);
				isSnapped = true;
			}
		}
	}

	if (!isSnapped)
	{
		Super::AdjustForGround(out_adjustedLocation, out_adjustedRotation);

		wall = nullptr;
	}

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("After AdjustForGround"));

		dumpDisqualifiers();

		//EC_LOG_Display_Condition(*getTagName(), TEXT("    After Adjusted location:  X = "), out_adjustedLocation.X, TEXT(" / Y = "), out_adjustedLocation.Y, TEXT(" / Z = "), out_adjustedLocation.Z);
		//EC_LOG_Display_Condition(*getTagName(), TEXT("    After Adjusted rotation: Pitch = "), out_adjustedRotation.Pitch, TEXT(" / Roll = "), out_adjustedRotation.Roll, TEXT(" / Yaw = "), out_adjustedRotation.Yaw);

		if (lastHit_.GetActor())
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *lastHit_.GetActor()->GetName());

			//EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
			//EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

			FRotator rotation = direction.Rotation();

			EC_LOG_Display_Condition(
				*getTagName(),
				TEXT("    Nearest location:  X = "),
				nearestCoord.X,
				TEXT(" / Y = "),
				nearestCoord.Y,
				TEXT(" / Z = "),
				nearestCoord.Z
				);
			EC_LOG_Display_Condition(
				*getTagName(),
				TEXT("    Rotation: Pitch = "),
				rotation.Pitch,
				TEXT(" / Roll = "),
				rotation.Roll,
				TEXT(" / Yaw = "),
				rotation.Yaw
				);
		}

		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("    Adjusted location:  X = "),
			out_adjustedLocation.X,
			TEXT(" / Y = "),
			out_adjustedLocation.Y,
			TEXT(" / Z = "),
			out_adjustedLocation.Z
			);
		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("    Adjusted rotation: Pitch = "),
			out_adjustedRotation.Pitch,
			TEXT(" / Roll = "),
			out_adjustedRotation.Roll,
			TEXT(" / Yaw = "),
			out_adjustedRotation.Yaw
			);

		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display_Condition(TEXT("===="));
	}
}

// bool AEfficiencyCheckerHologramWall::TrySnapToActor(const FHitResult& hitResult)
// {
//     static float lastCheck = 0;
//
//     bool ret = Super::TrySnapToActor(hitResult);
//
//     if (GetWorld()->TimeSince(lastCheck) > 10)
//     {
//         EC_LOG_Display_Condition(*getTagName(), TEXT("TrySnapToActor = "), ret);
//
//         dumpDisqualifiers();
//
//         {
//             EC_LOG_Display_Condition(*getTagName(), TEXT("Hologram = "), *GetName());
//
//             FVector location = GetActorLocation();
//             FRotator rotator = GetActorRotation();
//
//             EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
//             EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
//         }
//
//         if (hitResult.GetActor())
//         {
//             EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
//
//             FVector location = hitResult.GetActor()->GetActorLocation();
//             FRotator rotator = hitResult.GetActor()->GetActorRotation();
//
//             EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
//             EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
//
//             if (ret)
//             {
//                 EC_LOG_Display_Condition(*getTagName(), TEXT("    Snapping"));
//             }
//         }
//
//         lastCheck = GetWorld()->GetTimeSeconds();
//
//         EC_LOG_Display_Condition(TEXT("===="));
//     }
//
//     return ret;
// }

void AEfficiencyCheckerHologramWall::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	lastHit_ = hitResult;

	Super::SetHologramLocationAndRotation(hitResult);

	static float lastCheck = 0;

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("SetHologramLocationAndRotation"));

		dumpDisqualifiers();

		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display_Condition(TEXT("===="));
	}
}

void AEfficiencyCheckerHologramWall::CheckValidPlacement()
{
	static float lastCheck = 0;
	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Before CheckValidPlacement"));

		dumpDisqualifiers();
	}

	if (!wall)
	{
		Super::CheckValidPlacement();
	}

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("After CheckValidPlacement"));

		dumpDisqualifiers();

		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display_Condition(TEXT("===="));
	}
}

// ReSharper disable once IdentifierTypo
void AEfficiencyCheckerHologramWall::dumpDisqualifiers() const
{
	// ReSharper disable once IdentifierTypo
	for (const auto disqualifier : mConstructDisqualifiers)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Disqualifier "), *UFGConstructDisqualifier::GetDisqualifyingText(disqualifier).ToString());
	}
}

void AEfficiencyCheckerHologramWall::ScrollRotate(int32 delta, int32 step)
{
	static float lastCheck = 0;

	rotationDelta += delta;

	Super::ScrollRotate(delta, step);

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Scroll rotate delta = "), delta, TEXT(" / step = "), step);
	}
}

AEfficiencyCheckerBuilding* AEfficiencyCheckerHologramWall::GetCheckerBuildable() const
{
	if (mBuildClass->IsChildOf(AEfficiencyCheckerBuilding::StaticClass()))
	{
		AEfficiencyCheckerBuilding* cdo = mBuildClass->GetDefaultObject<AEfficiencyCheckerBuilding>();

		return cdo;
	}

	return nullptr;
}


#ifndef OPTIMIZE
#pragma optimize("", on)
#endif
