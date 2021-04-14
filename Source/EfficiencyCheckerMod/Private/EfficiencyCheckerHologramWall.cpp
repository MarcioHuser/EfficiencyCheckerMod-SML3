// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CommentTypo

#include "EfficiencyCheckerHologramWall.h"
#include "EfficiencyCheckerBuilding.h"
#include "Util/Logging.h"
#include "Util/Optimize.h"

#include "FGConstructDisqualifier.h"
#include "FGFactoryConnectionComponent.h"
#include "Buildables/FGBuildableWall.h"
#include "Components/WidgetComponent.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

AEfficiencyCheckerHologramWall::AEfficiencyCheckerHologramWall()
    : Super()
{
    this->mValidHitClasses.Add(AFGBuildableWall::StaticClass());
    this->mScrollMode = EHologramScrollMode::HSM_ROTATE;
}

// Called when the game starts or when spawned
void AEfficiencyCheckerHologramWall::BeginPlay()
{
    Super::BeginPlay();

    _TAG_NAME = GetName() + TEXT(": ");

    if (HasAuthority())
    {
        TInlineComponentArray<UWidgetComponent*> widgets;

        GetDefaultBuildable<AEfficiencyCheckerBuilding>()->GetComponents(widgets);

        for (auto widget : widgets)
        {
            widget->SetVisibility(false);
        }
    }
}

bool AEfficiencyCheckerHologramWall::IsValidHitResult(const FHitResult& hitResult) const
{
    const auto defaultBuildable = GetDefaultBuildable<AEfficiencyCheckerBuilding>();

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

        EC_LOG_Display(*getTagName(), TEXT("IsValidHitResult = "), ret);

        dumpDisqualifiers();

        if (hitResult.GetActor())
        {
            EC_LOG_Display(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());
            //
            // if (wall)
            // {
            //     EC_LOG_Display(*getTagName(), TEXT("Is Wall"));
            //
            //     for (const auto component : wall->GetComponents())
            //     {
            //         EC_LOG_Display(*getTagName(), TEXT("    "), *component->GetName(), TEXT(" / "), *GetPathNameSafe(component->GetClass()));
            //     }
            // }
        }

        EC_LOG_Display(TEXT("===="));
    }

    return ret;
}

void AEfficiencyCheckerHologramWall::AdjustForGround(const FHitResult& hitResult, FVector& out_adjustedLocation, FRotator& out_adjustedRotation)
{
    static float lastCheck = 0;

    if (GetWorld()->TimeSince(lastCheck) > 10)
    {
        EC_LOG_Display(*getTagName(), TEXT("Before AdjustForGround"));

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
    wall = nullptr;

    FVector nearestCoord;
    FVector direction;

    const auto defaultBuildable = GetDefaultBuildable<AEfficiencyCheckerBuilding>();

    if (defaultBuildable->resourceForm == EResourceForm::RF_SOLID)
    {
        wall = Cast<AFGBuildableWall>(hitResult.GetActor());

        if (wall)
        {
            nearestCoord = wall->GetActorLocation();
            direction = wall->GetActorRotation().Vector();

            TInlineComponentArray<UFGFactoryConnectionComponent*> components;
            wall->GetComponents(components);

            UFGFactoryConnectionComponent* nearestConnection = nullptr;

            for (const auto attachment : components)
            {
                if (FVector::Dist(attachment->GetComponentLocation(), hitResult.Location) <= 300 &&
                    (!nearestConnection || FVector::Dist(attachment->GetComponentLocation(), hitResult.Location) < FVector::Dist(
                        nearestConnection->GetComponentLocation(),
                        hitResult.Location
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
        Super::AdjustForGround(hitResult, out_adjustedLocation, out_adjustedRotation);

        wall = nullptr;
    }

    if (GetWorld()->TimeSince(lastCheck) > 10)
    {
        EC_LOG_Display(*getTagName(), TEXT("After AdjustForGround"));

        dumpDisqualifiers();

        //EC_LOG_Display(*getTagName(), TEXT("    After Adjusted location:  X = "), out_adjustedLocation.X, TEXT(" / Y = "), out_adjustedLocation.Y, TEXT(" / Z = "), out_adjustedLocation.Z);
        //EC_LOG_Display(*getTagName(), TEXT("    After Adjusted rotation: Pitch = "), out_adjustedRotation.Pitch, TEXT(" / Roll = "), out_adjustedRotation.Roll, TEXT(" / Yaw = "), out_adjustedRotation.Yaw);

        if (hitResult.GetActor())
        {
            EC_LOG_Display(*getTagName(), TEXT("Actor = "), *hitResult.GetActor()->GetName());

            //EC_LOG_Display(*getTagName(), TEXT("    X = "), location.X, TEXT(" / Y = "), location.Y, TEXT(" / Z = "), location.Z);
            //EC_LOG_Display(*getTagName(), TEXT("    Pitch = "), rotator.Pitch, TEXT(" / Roll = "), rotator.Roll, TEXT(" / Yaw = "), rotator.Yaw);

            FRotator rotation = direction.Rotation();

            EC_LOG_Display(*getTagName(), TEXT("    Nearest location:  X = "), nearestCoord.X, TEXT(" / Y = "), nearestCoord.Y, TEXT(" / Z = "), nearestCoord.Z);
            EC_LOG_Display(*getTagName(), TEXT("    Rotation: Pitch = "), rotation.Pitch, TEXT(" / Roll = "), rotation.Roll, TEXT(" / Yaw = "), rotation.Yaw);
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

// bool AEfficiencyCheckerHologramWall::TrySnapToActor(const FHitResult& hitResult)
// {
//     static float lastCheck = 0;
//
//     bool ret = Super::TrySnapToActor(hitResult);
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

void AEfficiencyCheckerHologramWall::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    Super::SetHologramLocationAndRotation(hitResult);

    static float lastCheck = 0;

    if (GetWorld()->TimeSince(lastCheck) > 10)
    {
        EC_LOG_Display(*getTagName(), TEXT("SetHologramLocationAndRotation"));

        dumpDisqualifiers();

        lastCheck = GetWorld()->GetTimeSeconds();

        EC_LOG_Display(TEXT("===="));
    }
}

void AEfficiencyCheckerHologramWall::CheckValidPlacement()
{
    static float lastCheck = 0;
    if (GetWorld()->TimeSince(lastCheck) > 10)
    {
        EC_LOG_Display(*getTagName(), TEXT("Before CheckValidPlacement"));

        dumpDisqualifiers();
    }

    if (!wall)
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
void AEfficiencyCheckerHologramWall::dumpDisqualifiers() const
{
    // ReSharper disable once IdentifierTypo
    for (const auto disqualifier : mConstructDisqualifiers)
    {
        EC_LOG_Display(*getTagName(), TEXT("Disqualifier "), *UFGConstructDisqualifier::GetDisqualifyingText(disqualifier).ToString());
    }
}

void AEfficiencyCheckerHologramWall::ScrollRotate(int32 delta, int32 step)
{
    static float lastCheck = 0;

    rotationDelta += delta;

    Super::ScrollRotate(delta, step);

    if (GetWorld()->TimeSince(lastCheck) > 10)
    {
        EC_LOG_Display(*getTagName(), TEXT("Scroll rotate delta = "), delta, TEXT(" / step = "), step);
    }
}

#ifndef OPTIMIZE
#pragma optimize( "", on)
#endif
