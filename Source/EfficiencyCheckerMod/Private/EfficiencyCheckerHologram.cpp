// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo

#include "EfficiencyCheckerHologram.h"

#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableRoad.h"
#include "Components/WidgetComponent.h"
#include "EfficiencyCheckerBuilding.h"
#include "FGConstructDisqualifier.h"
#include "FGPipeConnectionComponent.h"
#include "Util/EfficiencyCheckerOptimize.h"
#include "Util/Logging.h"

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

	EC_LOG_Display_Condition(
		*getTagName(),
		TEXT("BeginPlay / "),
		*getAuthorityAndPlayer(this)
		);

	if (HasAuthority())
	{
		const auto defaultBuildable = GetCheckerBuildable();

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
	const auto defaultBuildable = GetCheckerBuildable();

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

		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("IsValidHitResult = "),
			ret,
			TEXT(" / "),
			*getAuthorityAndPlayer(this)
			);

		if (hitResult.GetActor())
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
		}

		EC_LOG_Display_Condition(TEXT("===="));
	}

	return ret;
}

void AEfficiencyCheckerHologram::AdjustForGround(FVector& out_adjustedLocation, FRotator& out_adjustedRotation)
{
	const auto defaultBuildable = GetCheckerBuildable();

	static float lastCheck = 0;

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("Before AdjustForGround"),
			TEXT(" / "),
			*getAuthorityAndPlayer(this)
			);

		EC_LOG_Display_Condition(*getTagName(), TEXT("Hologram = "), *GetName());

		FVector location = GetActorLocation();
		FRotator rotator = GetActorRotation();

		EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
		EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

		if (lastHit_.GetActor())
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *lastHit_.GetActor()->GetName());

			location = lastHit_.GetActor()->GetActorLocation();
			rotator = lastHit_.GetActor()->GetActorRotation();

			EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
			EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);
		}
		else
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("No Actor"));
		}
	}

	bool isSnapped = false;
	efficiencyChecker = nullptr;
	pipeline = nullptr;
	conveyor = nullptr;

	FVector nearestCoord;
	FVector direction;

	{
		efficiencyChecker = Cast<AEfficiencyCheckerBuilding>(lastHit_.GetActor());
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
		conveyor = Cast<AFGBuildableConveyorBelt>(lastHit_.GetActor());
		if (conveyor)
		{
			splitOffset = conveyor->FindOffsetClosestToLocation(lastHit_.Location);

			conveyor->GetLocationAndDirectionAtOffset(conveyor->FindOffsetClosestToLocation(lastHit_.Location), nearestCoord, direction);

			out_adjustedRotation = direction.Rotation().Add(0, rotationDelta * 180, 0);
			out_adjustedLocation = nearestCoord + out_adjustedRotation.RotateVector(FVector(0, 0, -100)); // 1m bellow the belt

			isSnapped = true;
		}
	}

	if (!isSnapped && (defaultBuildable->resourceForm == EResourceForm::RF_LIQUID || defaultBuildable->resourceForm == EResourceForm::RF_GAS))
	{
		pipeline = Cast<AFGBuildablePipeline>(lastHit_.GetActor());
		if (pipeline)
		{
			splitOffset = pipeline->FindOffsetClosestToLocation(lastHit_.Location);

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
		Super::AdjustForGround(out_adjustedLocation, out_adjustedRotation);

		efficiencyChecker = nullptr;
		pipeline = nullptr;
		conveyor = nullptr;
	}

	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("After AdjustForGround"));

		//EC_LOG_Display_Condition(*getTagName(), TEXT("    After Adjusted location:  X = "), out_adjustedLocation.X, TEXT(" / Y = "), out_adjustedLocation.Y, TEXT(" / Z = "), out_adjustedLocation.Z);
		//EC_LOG_Display_Condition(*getTagName(), TEXT("    After Adjusted rotation: Pitch = "), out_adjustedRotation.Pitch, TEXT(" / Roll = "), out_adjustedRotation.Roll, TEXT(" / Yaw = "), out_adjustedRotation.Yaw);

		if (lastHit_.GetActor())
		{
			EC_LOG_Display_Condition(*getTagName(), TEXT("Actor = "), *lastHit_.GetActor()->GetName());

			//EC_LOG_Display_Condition(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
			//EC_LOG_Display_Condition(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

			FRotator rotation = direction.Rotation();

			if (isSnapped)
			{
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
				EC_LOG_Display_Condition(*getTagName(), TEXT("    Offset: "), splitOffset);
			}
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
//		EC_LOG_Display_Condition(*getTagName(), TEXT("GetRotationStep "), ret);
//
//		lastCheck = GetWorld()->GetTimeSeconds();
//
//		EC_LOG_Display_Condition(TEXT("===="));
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

void AEfficiencyCheckerHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
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

void AEfficiencyCheckerHologram::CheckValidPlacement()
{
	static float lastCheck = 0;
	if (GetWorld()->TimeSince(lastCheck) > 10)
	{
		EC_LOG_Display_Condition(*getTagName(), TEXT("Before CheckValidPlacement / "), *getAuthorityAndPlayer(this));

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
		EC_LOG_Display_Condition(*getTagName(), TEXT("After CheckValidPlacement"));

		dumpDisqualifiers();

		lastCheck = GetWorld()->GetTimeSeconds();

		EC_LOG_Display_Condition(TEXT("===="));
	}
}

// ReSharper disable once IdentifierTypo
void AEfficiencyCheckerHologram::dumpDisqualifiers() const
{
	// ReSharper disable once IdentifierTypo
	for (const auto disqualifier : mConstructDisqualifiers)
	{
		EC_LOG_Display_Condition(
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
	EC_LOG_Display_Condition(*getTagName(), TEXT("ConfigureComponents "), *GetPathNameSafe(inBuildable), TEXT(" / "), *getAuthorityAndPlayer(inBuildable));

	Super::ConfigureComponents(inBuildable);

	const auto newEfficiencyChecker = Cast<AEfficiencyCheckerBuilding>(inBuildable);
	if (newEfficiencyChecker)
	{
		newEfficiencyChecker->pipelineToSplit = pipeline;
		newEfficiencyChecker->pipelineSplitOffset = splitOffset;

		const auto defaultBuildable = GetCheckerBuildable();

		auto holoLoc = defaultBuildable->GetActorLocation();
		auto holoRot = defaultBuildable->GetActorRotation();

		EC_LOG_Display_Condition(*getTagName(), TEXT("Hologram position: x = "), holoLoc.X, TEXT(" / y = "), holoLoc.Y, TEXT(" / z = "), holoLoc.Z);
		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("Hologram rotation: pitch = "),
			holoRot.Pitch,
			TEXT(" / yaw = "),
			holoRot.Yaw,
			TEXT(" / roll = "),
			holoRot.Roll
			);

		auto newCheckLoc = newEfficiencyChecker->GetActorLocation();
		auto newCheckRot = newEfficiencyChecker->GetActorRotation();

		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("Checker position: x = "),
			newCheckLoc.X,
			TEXT(" / y = "),
			newCheckLoc.Y,
			TEXT(" / z = "),
			newCheckLoc.Z
			);
		EC_LOG_Display_Condition(
			*getTagName(),
			TEXT("Checker rotation: pitch = "),
			newCheckRot.Pitch,
			TEXT(" / yaw = "),
			newCheckRot.Yaw,
			TEXT(" / roll = "),
			newCheckRot.Roll
			);
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
		EC_LOG_Display_Condition(*getTagName(), TEXT("Scroll rotate delta = "), delta, TEXT(" / step = "), step);
	}
}

void AEfficiencyCheckerHologram::GetSupportedScrollModes(TArray<EHologramScrollMode>* out_modes) const
{
	if (out_modes)
	{
		out_modes->Add(EHologramScrollMode::HSM_ROTATE);
	}
}

AEfficiencyCheckerBuilding* AEfficiencyCheckerHologram::GetCheckerBuildable() const
{
	if (mBuildClass->IsChildOf(AEfficiencyCheckerBuilding::StaticClass()))
	{
		AEfficiencyCheckerBuilding* cdo = mBuildClass->GetDefaultObject<AEfficiencyCheckerBuilding>();

		return cdo;
	}

	return nullptr;
}

#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
