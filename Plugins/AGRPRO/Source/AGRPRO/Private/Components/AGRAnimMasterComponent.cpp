// Copyright 2021 Adam Grodzki All Rights Reserved.

#include "Components/AGRAnimMasterComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

#if WITH_EDITOR
#include "UI/AGRDebuggerController.h"
#include "UI/AGRUWDebugWidget.h"
#endif

UAGRAnimMasterComponent::UAGRAnimMasterComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	#if WITH_EDITOR
	bDebug = false;
	bLinePersists = false;
	LookAtLineColor = FColor::Blue;
	AimLineColor = FColor::Red;
	LineThickness = 1.0f;
	LineLifetime = 0.0f;
	#endif

	RotationMethod = ERotationMethod::RotateToVelocity;
	AimOffsetType = EAimOffsets::Look;
	AimOffsetBehavior = EAimOffsetClamp::Left;
	CameraBased = true;

	SetIsReplicatedByDefault(true);
}

void UAGRAnimMasterComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAGRAnimMasterComponent, BasePose);
	DOREPLIFETIME(UAGRAnimMasterComponent, OverlayPose);
	DOREPLIFETIME(UAGRAnimMasterComponent, RotationMethod);
	DOREPLIFETIME(UAGRAnimMasterComponent, RotationSpeed);
	DOREPLIFETIME(UAGRAnimMasterComponent, TurnStartAngle);
	DOREPLIFETIME(UAGRAnimMasterComponent, TurnStopTolerance);
	DOREPLIFETIME(UAGRAnimMasterComponent, AimOffsetType);
	DOREPLIFETIME(UAGRAnimMasterComponent, AimOffsetBehavior);
	DOREPLIFETIME(UAGRAnimMasterComponent, AnimModTags);
}

void UAGRAnimMasterComponent::BeginPlay()
{
	Super::BeginPlay();

	RecastOwner();
	SetupBasePose(BasePose);
	SetupOverlayPose(OverlayPose);
	SetupFpp(bFirstPerson);
	SetupRotation(RotationMethod, RotationSpeed, TurnStartAngle, TurnStopTolerance);
	SetupAimOffset(AimOffsetType, AimOffsetBehavior, AimClamp, CameraBased, AimSocketName, LookAtSocketName);

	#if WITH_EDITOR
	SetupGameplayDebug();
	#endif
}

void UAGRAnimMasterComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(!IsValid(OwningCharacter))
	{
		return;
	}

	AimTick();
	TurnInPlaceTick();
}

void UAGRAnimMasterComponent::SetupBasePose(FGameplayTag InBasePose)
{
	BasePose = InBasePose;
	ServerSetBasePose(InBasePose);
}

void UAGRAnimMasterComponent::SetupOverlayPose(FGameplayTag InOverlayPose)
{
	OverlayPose = InOverlayPose;
	ServerSetOverlayPose(InOverlayPose);
}

void UAGRAnimMasterComponent::SetupFpp(bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
	OnFirstPerson.Broadcast(bInFirstPerson);
}

void UAGRAnimMasterComponent::SetupRotation(
	const ERotationMethod InRotationMethod,
	const float InRotationSpeed,
	const float InTurnStartAngle,
	const float InTurnStopTolerance)
{
	RotationMethod = InRotationMethod;
	HandleRotationMethodChange();
	RotationSpeed = InRotationSpeed;
	HandleRotationSpeedChange();
	TurnStartAngle = InTurnStartAngle;
	TurnStopTolerance = InTurnStopTolerance;

	ServerSetRotation(InRotationMethod, InRotationSpeed, InTurnStartAngle, InTurnStopTolerance);
}

void UAGRAnimMasterComponent::SetupAimOffset(
        const EAimOffsets InAimOffsetType,
        const EAimOffsetClamp InAimBehavior,
        const float InAimClamp,
        const bool InCameraBased,
        const FName InAimSocketName,
        const FName InLookAtSocketName)
{
	AimOffsetType = InAimOffsetType;
	AimOffsetBehavior = InAimBehavior;
	AimClamp = InAimClamp;
	CameraBased = InCameraBased;
	AimSocketName = InAimSocketName;
	LookAtSocketName = InLookAtSocketName;

	ServerSetupAimOffset(InAimOffsetType, InAimBehavior);
}

void UAGRAnimMasterComponent::AddTag(FGameplayTag InTag)
{
	AnimModTags.AddTag(InTag);
}

bool UAGRAnimMasterComponent::RemoveTag(FGameplayTag InTag)
{
	return AnimModTags.RemoveTag(InTag);
}

void UAGRAnimMasterComponent::AimTick()
{
	if(OwningCharacter && OwningCharacter->IsLocallyControlled())
	{
		if(OwningCharacter->IsPlayerControlled() && CameraBased)
		{
			LookAtIfPlayerControlled();
		}
		else
		{
			AimOffset = OwningCharacter->GetControlRotation();
			ServerSetAimOffset(AimOffset);
			LookAtWithoutCamera();
		}
	}
}

void UAGRAnimMasterComponent::TurnInPlaceTick()
{
	if(RotationMethod == ERotationMethod::DesiredAtAngle)
	{
		const float Delta = UKismetMathLibrary::NormalizedDeltaRotator(AimOffset, GetOwner()->GetActorRotation()).Yaw;
		const float AbsoluteDelta = UKismetMathLibrary::Abs(Delta);
		const float Speed = GetOwner()->GetVelocity().Size();
		if(AbsoluteDelta > TurnStartAngle || Speed > 25.0f)
		{
			OwnerMovementComponent->bUseControllerDesiredRotation = true;
		}
		else
		{
			const float ClampValue = FMath::Clamp(TurnStopTolerance, 1.0f, 90.0f);
			const float Min = ClampValue/-1.0f;
			const float Max = ClampValue/1.0f;
			const bool bInRange = UKismetMathLibrary::InRange_FloatFloat(Delta, Min, Max, true, true);
			if(bInRange)
			{
				OwnerMovementComponent->bUseControllerDesiredRotation = false;
			}
		}
	}
}

void UAGRAnimMasterComponent::RecastOwner()
{
	// If the owner is invalid, return.
	if(!GetOwner())
	{
		return;
	}
	OwningCharacter = Cast<ACharacter>(GetOwner());


	// If the owning character is invalid, return.
	if(!OwningCharacter)
	{
		return;
	}

	OwnerMovementComponent = OwningCharacter->GetCharacterMovement();
}

void UAGRAnimMasterComponent::HandleRotationSpeedChange()
{
	if(!OwnerMovementComponent)
	{
		RecastOwner();
	}
	OwnerMovementComponent->RotationRate.Yaw = RotationSpeed;
}

void UAGRAnimMasterComponent::HandleRotationMethodChange()
{
	if(!OwningCharacter)
	{
		RecastOwner();
	}

	switch(RotationMethod)
	{
		case ERotationMethod::NONE:
			OwningCharacter->bUseControllerRotationYaw = false;
			OwnerMovementComponent->bOrientRotationToMovement = false;
			OwnerMovementComponent->bUseControllerDesiredRotation = false;
			break;
		case ERotationMethod::RotateToVelocity:
			OwningCharacter->bUseControllerRotationYaw = false;
			OwnerMovementComponent->bOrientRotationToMovement = true;
			OwnerMovementComponent->bUseControllerDesiredRotation = false;
			break;
		case ERotationMethod::DesiredAtAngle:
			OwningCharacter->bUseControllerRotationYaw = false;
			OwnerMovementComponent->bOrientRotationToMovement = false;
			OwnerMovementComponent->bUseControllerDesiredRotation = false;
			break;
		case ERotationMethod::DesiredRotation:
			OwningCharacter->bUseControllerRotationYaw = false;
			OwnerMovementComponent->bOrientRotationToMovement = false;
			OwnerMovementComponent->bUseControllerDesiredRotation = true;
			break;
		case ERotationMethod::AbsoluteRotation:
			OwningCharacter->bUseControllerRotationYaw = true;
			OwnerMovementComponent->bOrientRotationToMovement = false;
			OwnerMovementComponent->bUseControllerDesiredRotation = false;
			break;

		default:
			checkNoEntry();
	}
}

void UAGRAnimMasterComponent::LookAtIfPlayerControlled()
{
	APlayerCameraManager* PlayerCam = UGameplayStatics::GetPlayerCameraManager(this, 0);

	if (!IsValid(PlayerCam))
	{
		return;
	}

	FHitResult HitResult;

	FVector Start = PlayerCam->GetCameraLocation();
	FVector End = Start + (PlayerCam->GetCameraRotation().Vector() * 10000.0f);

	FCollisionQueryParams QueryParams;

	TArray<AActor*> IgnoredActors;
	OwningCharacter->GetAttachedActors(IgnoredActors, true);
	QueryParams.AddIgnoredActors(IgnoredActors);
	QueryParams.AddIgnoredActor(GetOwner());

	QueryParams.bTraceComplex = true;
	#if WITH_EDITOR
	QueryParams.bDebugQuery = bDebug;
	#endif

	const FName Socket = AimOffsetType == EAimOffsets::Aim ? AimSocketName : LookAtSocketName;

	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, TraceChannel, QueryParams);
	if(bHit)
	{
		LookAtLocation = HitResult.ImpactPoint;
		ServerSetLookAt(LookAtLocation);
		AimOffset = UKismetMathLibrary::FindLookAtRotation(OwningCharacter->GetMesh()->GetSocketLocation(Socket), HitResult.ImpactPoint);
		ServerSetAimOffset(AimOffset);

		#if WITH_EDITOR
		if(bDebug)
		{
			Start =  OwningCharacter->GetMesh()->GetSocketLocation(LookAtSocketName);
			End = HitResult.ImpactPoint;
			DrawDebugLine(GetWorld(), Start, End, LookAtLineColor, bLinePersists, LineLifetime, 0, LineThickness);

			Start =  OwningCharacter->GetMesh()->GetSocketLocation(AimSocketName);
			DrawDebugLine(GetWorld(), Start, End, AimLineColor,bLinePersists, LineLifetime, 0, LineThickness);
		}
		#endif
	}
	else
	{
		LookAtLocation = HitResult.TraceEnd;
		ServerSetLookAt(LookAtLocation);
		AimOffset = UKismetMathLibrary::FindLookAtRotation(OwningCharacter->GetMesh()->GetSocketLocation(Socket), HitResult.TraceEnd);
		ServerSetAimOffset(AimOffset);

		#if WITH_EDITOR
		if(bDebug)
		{
			Start =  OwningCharacter->GetMesh()->GetSocketLocation(LookAtSocketName);
			End = HitResult.TraceEnd;
			DrawDebugLine(GetWorld(), Start, End, LookAtLineColor, bLinePersists, LineLifetime, 0, LineThickness);

			Start =  OwningCharacter->GetMesh()->GetSocketLocation(AimSocketName);
			DrawDebugLine(GetWorld(), Start, End, AimLineColor, bLinePersists, LineLifetime, 0, LineThickness);
		}
		#endif
	}
}

void UAGRAnimMasterComponent::LookAtWithoutCamera()
{
	FHitResult HitResult;
	float offsetStart = 10000.0f;

	FCollisionQueryParams QueryParams;
	#if WITH_EDITOR
	QueryParams.bDebugQuery = bDebug;
	#endif
	QueryParams.bTraceComplex = true;

	TArray<AActor*> IgnoredActors;
	OwningCharacter->GetAttachedActors(IgnoredActors, true);
	QueryParams.AddIgnoredActors(IgnoredActors);
	QueryParams.AddIgnoredActor(GetOwner());

	FName Socket = LookAtSocketName;
	FVector Start = OwningCharacter->GetMesh()->GetSocketLocation(Socket);
	FVector End = (UKismetMathLibrary::GetForwardVector(OwningCharacter->GetController()->GetControlRotation()) * offsetStart) + Start;

	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, TraceChannel, QueryParams);
	if (bHit)
	{
		LookAtLocation = HitResult.ImpactPoint;
		ServerSetLookAt(LookAtLocation);

		#if WITH_EDITOR
		if (bDebug)
		{
			Start = OwningCharacter->GetMesh()->GetSocketLocation(LookAtSocketName);
			End = HitResult.ImpactPoint;
			DrawDebugLine(GetWorld(), Start, End, LookAtLineColor, bLinePersists, LineLifetime, 0, LineThickness);

			Start = OwningCharacter->GetMesh()->GetSocketLocation(AimSocketName);
			DrawDebugLine(GetWorld(), Start, End, AimLineColor, bLinePersists, LineLifetime, 0, LineThickness);
		}
		#endif
	}
	else
	{
		LookAtLocation = HitResult.TraceEnd;
		ServerSetLookAt(LookAtLocation);

		#if WITH_EDITOR
		if (bDebug)
		{
			Start = OwningCharacter->GetMesh()->GetSocketLocation(LookAtSocketName);
			End = HitResult.TraceEnd;
			DrawDebugLine(GetWorld(), Start, End, LookAtLineColor, bLinePersists, LineLifetime, 0, LineThickness);

			Start = OwningCharacter->GetMesh()->GetSocketLocation(AimSocketName);
			DrawDebugLine(GetWorld(), Start, End, AimLineColor, bLinePersists, LineLifetime, 0, LineThickness);
		}
		#endif
	}
}

void UAGRAnimMasterComponent::OnRep_RotationMethod()
{
	HandleRotationMethodChange();
}

void UAGRAnimMasterComponent::OnRep_RotationSpeed()
{
	HandleRotationSpeedChange();
}

void UAGRAnimMasterComponent::ServerSetAimOffset_Implementation(const FRotator InAimOffset)
{
	NetMutiSetAimOffset(InAimOffset);
}

void UAGRAnimMasterComponent::NetMutiSetAimOffset_Implementation(const FRotator InAimOffset)
{
	if(OwningCharacter && !OwningCharacter->IsLocallyControlled())
	{
		AimOffset = InAimOffset;
	}
}

void UAGRAnimMasterComponent::ServerSetupAimOffset_Implementation(const EAimOffsets InAimOffsetType, const EAimOffsetClamp InAimBehavior)
{
	AimOffsetType = InAimOffsetType;
	AimOffsetBehavior = InAimBehavior;
}

void UAGRAnimMasterComponent::ServerSetRotation_Implementation(
	const ERotationMethod InRotationMethod,
	const float InRotationSpeed,
	const float InTurnStartAngle,
	const float InTurnStopTolerance)
{
	RotationMethod = InRotationMethod;
	HandleRotationMethodChange();
	RotationSpeed = InRotationSpeed;
	HandleRotationSpeedChange();
	TurnStartAngle = InTurnStartAngle;
	TurnStopTolerance = InTurnStopTolerance;
}

void UAGRAnimMasterComponent::ServerSetOverlayPose_Implementation(const FGameplayTag InOverlayPose)
{
	OverlayPose = InOverlayPose;
}

void UAGRAnimMasterComponent::ServerSetBasePose_Implementation(const FGameplayTag InBasePose)
{
	BasePose = InBasePose;
}

void UAGRAnimMasterComponent::ServerSetLookAt_Implementation(const FVector LookAt)
{
	NetMutiSetLookAt(LookAt);
}

void UAGRAnimMasterComponent::NetMutiSetLookAt_Implementation(const FVector LookAt)
{
	if (OwningCharacter && !OwningCharacter->IsLocallyControlled())
	{
		LookAtLocation = LookAt;
	}
}

#if WITH_EDITOR
void UAGRAnimMasterComponent::SetupGameplayDebug()
{
	if (!IsValid(OwningCharacter))
	{
		return;
	}

	APlayerController* OwnerPC = Cast<APlayerController>(OwningCharacter->GetController());
	if (!IsValid(OwnerPC))
	{
		return;
	}

	UInputComponent* InputComponent = NewObject<UInputComponent>(OwnerPC, TEXT("AGR_GameplayDebug_Input"));
	InputComponent->Priority = -1;

	// keep debugger controller objects for easy access and GC
	DebuggerController = NewObject<UAGRDebuggerController>(OwnerPC, TEXT("AGR_GameplayDebug_Controller"));
	DebuggerController->Initialize(OwnerPC);
	DebuggerController->BindInputs(*InputComponent);

	OwnerPC->PushInputComponent(InputComponent);
}
#endif