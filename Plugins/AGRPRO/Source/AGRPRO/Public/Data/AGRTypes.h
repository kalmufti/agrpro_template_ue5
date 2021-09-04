// Copyright 2021 Adam Grodzki All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "AGRTypes.generated.h"

UENUM(BlueprintType)
enum class EAimOffsetClamp:uint8
{
	Nearest = 0 UMETA(DisplayName = "Nearest"),
	Left		UMETA(DisplayName = "Left"),
	Right		UMETA(DisplayName = "Right")
};

UENUM(BlueprintType)
enum class EAimOffsets:uint8
{
	NONE = 0	UMETA(DisplayName = "NONE"),
	Aim			UMETA(DisplayName = "Aim"),
	Look		UMETA(DisplayName = "Look")
};

UENUM(BlueprintType)
enum class ERotationMethod:uint8
{
	NONE = 0			UMETA(DisplayName = "NONE"),
	RotateToVelocity	UMETA(DisplayName = "Rotate To Velocity"),
	AbsoluteRotation	UMETA(DisplayName = "Absolute Rotation"),
	DesiredRotation		UMETA(DisplayName = "Desired Rotation"),
	DesiredAtAngle		UMETA(DisplayName = "Desired At Angle")
};

USTRUCT(BlueprintType)
struct FEquipment
{
	GENERATED_BODY();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AGR")
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AGR")
	FGameplayTagContainer AcceptableSlots;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AGR")
	AActor* ItemActor = nullptr;
};
