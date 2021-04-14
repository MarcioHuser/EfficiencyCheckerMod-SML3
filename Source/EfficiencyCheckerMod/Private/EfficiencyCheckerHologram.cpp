// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo

#include "EfficiencyCheckerHologram.h"
#include "EfficiencyCheckerBuilding.h"
#include "Util/Logging.h"
#include "Util/Optimize.h"

#include "FGConstructDisqualifier.h"
#include "FGPipeConnectionComponent.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableRoad.h"
#include "Components/WidgetComponent.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

FString AEfficiencyCheckerHologram::getAuthorityAndPlayer(const AActor* actor)
{
	return FString(TEXT("Has Authority = ")) +
		(actor->HasAuthority() ? TEXT("true") : TEXT("false")) +
		TEXT(" / Character = ") +
		actor->GetWorld()->GetFirstPlayerController()->GetCharacter()->GetHumanReadableName();
}

AEfficiencyCheckerHologram::AEfficiencyCheckerHologram()
	: Super()
{
	this->mValidHitClasses.Add(AFGBuildableFoundation::StaticClass());
	this->mValidHitClasses.Add(AFGBuildableRoad::StaticClass());
	this->mValidHitClasses.Add(AEfficiencyCheckerBuilding::StaticClass());
	this->mValidHitClasses.Add(AFGBuildableConveyorBelt::StaticClass());
	this->mValidHitClasses.Add(AFGBuildablePipeline::StaticClass());
	this->mScrollMode = EHologramScrollMode::HSM_ROTATE;
}

// Called when the game starts or when spawned
void AEfficiencyCheckerHologram::BeginPlay()
{
	Super::BeginPlay();

	_TAG_NAME = GetName() + TEXT(": ");

	EC_LOG_Display(
		*getTagName(),
		TEXT("BeginPlay / "),
		*getAuthorityAndPlayer(this)
		);

	if (HasAuthority())
	{
		const auto defaultBuildable = GetDefaultBuildable<AEfficiencyCheckerBuilding>();

		TInlineComponentArray<UWidgetComponent*> widgets(defaultBuildable, true);
		for (auto widget : widgets)
		{
			widget->SetVisibility(false);
		}

		auto arrows = defaultBuildable->GetComponentsByTag(UStaticMeshComponent::StaticClass(), TEXT("DirectionArrow"));
		for (auto arrow : arrows)
		{
			Cast<UStaticMeshComponent>(arrow)->SetVisibility(true);
		}
	}
}

bool AEfficiencyCheckerHologram::IsValidHitResult(const FHitResult& hitResult) const
{
	const auto defaultBuildable = GetDefaultBuildable<AEfficiencyCheckerBuilding>();

	bool ret = Super::IsValidHitResult(hitResult);

	if (ret)
	{
		AEfficiencyCheckerBuilding* ec = Cast<AEfficiencyCheckerBuilding>(hitResult.GetActor());
		if (ec)
		{
			ret = ec->GetActorRotation().Pitch == 0 && ec->GetActorRotation().Roll == 0;
		}
	}

	if (!ret)
	{
		ret = defaultBuildable->resourceForm == EResourceForm::RF_SOLID && Cast<AFGBuildableConveyorBelt>(hitResult.GetActor());
	}

	if (!ret)
	{
		ret = (defaultBuildable->resourceForm == EResourceForm::RF_LIQUID || defaultBuildable->resourceForm == EResourceForm::RF_GAS) &&
			Cast<AFGBuildablePipeline>(hitResult.GetActor());
	}

	static float lastCheck = 0;

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display(
			*getTagName(),
			TEXT("IsValidHitResult = "),
			ret,
			TEXT(" / "),
			*getAuthorityAndPlayer(this)
			);

		if (hitResult.GetActor())
		{
			EC_LOG_Display(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
		}

		EC_LOG_Display(TEXT("===="));
	}

	return ret;
}

void AEfficiencyCheckerHologram::AdjustForGround(const FHitResult& hitResult, FVector& out_adjustedLocation, FRotator& out_adjustedRotation)
{
	const auto defaultBuildable = GetDefaultBuildable<AEfficiencyCheckerBuilding>();

	static float lastCheck = 0;

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display(
			*getTagName(),
			TEXT("Before AdjustForGround"),
			TEXT(" / "),
			*getAuthorityAndPlayer(this)
			);

		EC_LOG_Display(*getTagName(), TEXT("Hologram = "), *GetName());

		FVector location = GetActorLocation();
		FRotator rotator = GetActorRotation();

		EC_LOG_Display(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
		EC_LOG_Display(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

		EC_LOG_Display(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());

		location = hitResult.GetActor()->GetActorLocation();
		rotator = hitResult.GetActor()->GetActorRotation();

		EC_LOG_Display(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
		EC_LOG_Display(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
	}

	bool isSnapped = false;
	efficiencyChecker = nullptr;
	pipeline = nullptr;
	conveyor = nullptr;

	FVector nearestCoord;
	FVector direction;

	{
		efficiencyChecker = Cast<AEfficiencyCheckerBuilding>(hitResult.GetActor());
		if (efficiencyChecker && efficiencyChecker->GetActorRotation().Pitch == 0 && efficiencyChecker->GetActorRotation().Roll == 0)
		{
			nearestCoord = efficiencyChecker->GetActorLocation();

			out_adjustedLocation = efficiencyChecker->GetActorLocation() + FVector(0, 0, 200); // Two metters above the origin

			out_adjustedRotation = efficiencyChecker->GetActorRotation().Add(0, rotationDelta * 90, 0);

			isSnapped = true;
		}
		else
		{
			efficiencyChecker = nullptr;
		}
	}

	splitOffset = 0;

	if (!isSnapped && defaultBuildable->resourceForm == EResourceForm::RF_SOLID)
	{
		conveyor = Cast<AFGBuildableConveyorBelt>(hitResult.GetActor());
		if (conveyor)
		{
			splitOffset = conveyor->FindOffsetClosestToLocation(hitResult.Location);

			conveyor->GetLocationAndDirectionAtOffset(conveyor->FindOffsetClosestToLocation(hitResult.Location), nearestCoord, direction);

			out_adjustedRotation = direction.Rotation().Add(0, rotationDelta * 180, 0);
			out_adjustedLocation = nearestCoord + out_adjustedRotation.RotateVector(FVector(0, 0, -100)); // 1m bellow the belt

			isSnapped = true;
		}
	}

	if (!isSnapped && (defaultBuildable->resourceForm == EResourceForm::RF_LIQUID || defaultBuildable->resourceForm == EResourceForm::RF_GAS))
	{
		pipeline = Cast<AFGBuildablePipeline>(hitResult.GetActor());
		if (pipeline)
		{
			splitOffset = pipeline->FindOffsetClosestToLocation(hitResult.Location);

			pipeline->GetLocationAndDirectionAtOffset(splitOffset, nearestCoord, direction);

			if (rotationDelta &&
				(GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::LeftControl) || GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::R)))
			{
				rotationDelta = 0;
			}

			// Check if it too close to an edge
			if (splitOffset <= 150 && !pipeline->GetPipeConnection0()->IsConnected() ||
				pipeline->GetLength() - 150 <= splitOffset && !pipeline->GetPipeConnection1()->IsConnected())
			{
				const auto closestPipeConnection = splitOffset <= 150 ? pipeline->GetPipeConnection0() : pipeline->GetPipeConnection1();

				// Snap to the edge
				out_adjustedRotation = closestPipeConnection->GetComponentRotation().Add(0, 0, rotationDelta * 90.0 / 4);
				out_adjustedLocation = closestPipeConnection->GetConnectorLocation() + out_adjustedRotation.RotateVector(FVector(100, 0, -175));
			}
			else
			{
				// Will split
				out_adjustedRotation = direction.Rotation().Add(0, 0, rotationDelta * 90.0 / 4);
				out_adjustedLocation = nearestCoord + out_adjustedRotation.RotateVector(FVector(0, 0, -175));
			}

			isSnapped = true;
		}
	}

	if (!isSnapped)
	{
		Super::AdjustForGround(hitResult, out_adjustedLocation, out_adjustedRotation);

		efficiencyChecker = nullptr;
		pipeline = nullptr;
		conveyor = nullptr;
	}

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display(*getTagName(), TEXT("After AdjustForGround"));

		//EC_LOG_Display(*getTagName(), TEXT("    After Adjusted location:  X = "), out_adjustedLocation.X, TEXT(" / Y = "), out_adjustedLocation.Y, TEXT(" / Z = "), out_adjustedLocation.Z);
		//EC_LOG_Display(*getTagName(), TEXT("    After Adjusted rotation: Pitch = "), out_adjustedRotation.Pitch, TEXT(" / Roll = "), out_adjustedRotation.Roll, TEXT(" / Yaw = "), out_adjustedRotation.Yaw);

		if (hitResult.GetActor())
		{
			EC_LOG_Display(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());

			//EC_LOG_Display(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
			//EC_LOG_Display(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

			FRotator rotation = direction.Rotation();

			if (isSnapped)
			{
				EC_LOG_Display(*getTagName(), TEXT("    Nearest location:  X = "), nearestCoord.X, TEXT(" / Y = "), nearestCoord.Y, TEXT(" / Z = "), nearestCoord.Z);
				EC_LOG_Display(*getTagName(), TEXT("    Rotation: Pitch = "), rotation.Pitch, TEXT(" / Roll = "), rotation.Roll, TEXT(" / Yaw = "), rotation.Yaw);
				EC_LOG_Display(*getTagName(), TEXT("    Offset: "), splitOffset);
			}
		}

		EC_LOG_Display(
			*getTagName(),
			TEXT("    Adjusted location:  X = "),
			out_adjustedLocation.X,
			TEXT(" / Y = "),
			out_adjustedLocation.Y,
			TEXT(" / Z = "),
			out_adjustedLocation.Z
			);
		EC_LOG_Display(
			*getTagName(),
			TEXT("    Adjusted rotation: Pitch = "),
			out_adjustedRotation.Pitch,
			TEXT(" / Roll = "),
			out_adjustedRotation.Roll,
			TEXT(" / Yaw = "),
			out_adjustedRotation.Yaw
			);


		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display(TEXT("===="));
	}
}

//
//int32 AEfficiencyCheckerHologram::GetRotationStep() const
//{
//	static float lastCheck = 0;
//
//	int32 ret;
//
//	//if (isEfficiencyChecker) {
//	//	ret = 90;
//	//}
//	//else {
//		ret = Super::GetRotationStep();
//	//}
//
//	if (GetWorld()->TimeSince(lastCheck) > 10) {
//		EC_LOG_Display(*getTagName(), TEXT("GetRotationStep "), ret);
//
//		lastCheck = GetWorld()->GetTimeSeconds();
//
//		EC_LOG_Display(TEXT("===="));
//	}
//
//	return ret;
//}

// bool AEfficiencyCheckerHologram::TrySnapToActor(const FHitResult& hitResult)
// {
//     static float lastCheck = 0;
//
//     bool ret;
//
//     if (efficiencyChecker || pipeline || conveyor)
//     {
//         ret = true;
//     }
//     else
//     {
//         ret = Super::TrySnapToActor(hitResult);
//     }
//
//     if (GetWorld()->TimeSince(lastCheck) > 10)
//     {
//         EC_LOG_Display(*getTagName(), TEXT("TrySnapToActor = "), ret);
//
//         dumpDisqualifiers();
//
//         {
//             EC_LOG_Display(*getTagName(), TEXT("Hologram = "), *GetName());
//
//             FVector location = GetActorLocation();
//             FRotator rotator = GetActorRotation();
//
//             EC_LOG_Display(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
//             EC_LOG_Display(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
//         }
//
//         if (hitResult.GetActor())
//         {
//             EC_LOG_Display(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
//
//             FVector location = hitResult.GetActor()->GetActorLocation();
//             FRotator rotator = hitResult.GetActor()->GetActorRotation();
//
//             EC_LOG_Display(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
//             EC_LOG_Display(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
//
//             if (ret)
//             {
//                 EC_LOG_Display(*getTagName(), TEXT("    Snapping"));
//             }
//         }
//
//         lastCheck = GetWorld()->GetTimeSeconds();
//
//         EC_LOG_Display(TEXT("===="));
//     }
//
//     return ret;
// }

// void AEfficiencyCheckerHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
// {
//     Super::SetHologramLocationAndRotation(hitResult);
//
//     static float lastCheck = 0;
//
//     if (GetWorld()->TimeSince(lastCheck) > 10)
//     {
//         EC_LOG_Display(*getTagName(), TEXT("SetHologramLocationAndRotation"));
//
//         dumpDisqualifiers();
//
//         lastCheck = GetWorld()->GetTimeSeconds();
//
//         EC_LOG_Display(TEXT("===="));
//     }
// }

void AEfficiencyCheckerHologram::CheckValidPlacement()
{
	static float lastCheck = 0;
	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display(*getTagName(), TEXT("Before CheckValidPlacement / "), *getAuthorityAndPlayer(this));

		dumpDisqualifiers();
	}

	if (pipeline)
	{
		if (150 < splitOffset && splitOffset <= pipeline->GetLength() - 150)
		{
			FVector previousDirection;
			FVector previousPoint;
			FVector nextDirection;
			FVector nextPoint;

			pipeline->GetLocationAndDirectionAtOffset(splitOffset - 100, previousPoint, previousDirection);
			pipeline->GetLocationAndDirectionAtOffset(splitOffset + 100, nextPoint, nextDirection);

			if (!FVector::Coincident(previousDirection, nextDirection) ||
				!FVector::Parallel(nextPoint - previousPoint, previousDirection))
			{
				AddConstructDisqualifier(UFGCDPipeAttachmentTooSharpTurn::StaticClass());
			}
		}
		else if (splitOffset <= 150 && pipeline->GetPipeConnection0()->IsConnected() ||
			pipeline->GetLength() - 150 <= splitOffset && pipeline->GetPipeConnection1()->IsConnected())
		{
			if (pipeline->GetPipeConnection0()->IsConnected())
			{
				AddConstructDisqualifier(UFGCDEncroachingClearance::StaticClass());
			}
		}
	}
	else if (!efficiencyChecker && !pipeline && !conveyor)
	{
		Super::CheckValidPlacement();
	}

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display(*getTagName(), TEXT("After CheckValidPlacement"));

		dumpDisqualifiers();

		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display(TEXT("===="));
	}
}

// ReSharper disable once IdentifierTypo
void AEfficiencyCheckerHologram::dumpDisqualifiers() const
{
	// ReSharper disable once IdentifierTypo
	for (const auto disqualifier : mConstructDisqualifiers)
	{
		EC_LOG_Display(
			*getTagName(),
			TEXT("Disqualifier "),
			*UFGConstructDisqualifier::GetDisqualifyingText(disqualifier).ToString(),
			TEXT(" / "),
			*disqualifier->GetFullName()
			);
	}
}

void AEfficiencyCheckerHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
	EC_LOG_Display(*getTagName(), TEXT("ConfigureComponents "), *GetPathNameSafe(inBuildable), TEXT(" / "), *getAuthorityAndPlayer(inBuildable));

	Super::ConfigureComponents(inBuildable);

	const auto newEfficiencyChecker = Cast<AEfficiencyCheckerBuilding>(inBuildable);
	if (newEfficiencyChecker)
	{
		newEfficiencyChecker->pipelineToSplit = pipeline;
		newEfficiencyChecker->pipelineSplitOffset = splitOffset;

		const auto defaultBuildable = GetDefaultBuildable<AEfficiencyCheckerBuilding>();

		auto holoLoc = defaultBuildable->GetActorLocation();
		auto holoRot = defaultBuildable->GetActorRotation();

		EC_LOG_Display(*getTagName(), TEXT("Hologram position: x = "), holoLoc.X, TEXT(" / y = "), holoLoc.Y, TEXT(" / z = "), holoLoc.Z);
		EC_LOG_Display(*getTagName(), TEXT("Hologram rotation: pitch = "), holoRot.Pitch, TEXT(" / yaw = "), holoRot.Yaw, TEXT(" / roll = "), holoRot.Roll);

		auto newCheckLoc = newEfficiencyChecker->GetActorLocation();
		auto newCheckRot = newEfficiencyChecker->GetActorRotation();

		EC_LOG_Display(*getTagName(), TEXT("Checker position: x = "), newCheckLoc.X, TEXT(" / y = "), newCheckLoc.Y, TEXT(" / z = "), newCheckLoc.Z);
		EC_LOG_Display(*getTagName(), TEXT("Checker rotation: pitch = "), newCheckRot.Pitch, TEXT(" / yaw = "), newCheckRot.Yaw, TEXT(" / roll = "), newCheckRot.Roll);
	}
}

void AEfficiencyCheckerHologram::ScrollRotate(int32 delta, int32 step)
{
	static float lastCheck = 0;

	if (efficiencyChecker)
	{
		rotationDelta += delta;
	}
	else if (pipeline)
	{
		rotationDelta += delta;
	}
	else
	{
		Super::ScrollRotate(delta, step);
	}

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display(*getTagName(), TEXT("Scroll rotate delta = "), delta, TEXT(" / step = "), step);
	}
}

void AEfficiencyCheckerHologram::GetSupportedScrollModes(TArray<EHologramScrollMode>* out_modes) const
{
	if (out_modes)
	{
		out_modes->Add(EHologramScrollMode::HSM_ROTATE);
	}
}

#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
