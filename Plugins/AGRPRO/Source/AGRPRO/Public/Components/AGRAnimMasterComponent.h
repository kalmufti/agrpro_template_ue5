// Copyright 2021 Adam Grodzki All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/AGRTypes.h"
#include "GameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "UI/AGRDebuggerController.h"
#endif

#include "AGRAnimMasterComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFirstPerson, bool, bIsFirstPersonView);

UCLASS(BlueprintType, Blueprintable, ClassGroup=("AGR"), meta=(BlueprintSpawnableComponent) )
class AGRPRO_API UAGRAnimMasterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = "AGR|Setup")
	FGameplayTag BasePose;

	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = "AGR|Setup")
	FGameplayTag OverlayPose;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AGR|Runtime")
	bool bFirstPerson;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AGR|Runtime")
	FRotator AimOffset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AGR|Runtime")
	FVector LookAtLocation;

	UPROPERTY(BlueprintReadWrite, Replicated, EditAnywhere, Category = "AGR|Setup")
	FGameplayTagContainer AnimModTags;

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_RotationMethod, EditDefaultsOnly, Category = "AGR|Setup")
	ERotationMethod RotationMethod = ERotationMethod::NONE;

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_RotationSpeed, EditDefaultsOnly, Category = "AGR|Setup")
	float RotationSpeed = 360.0f;

	UPROPERTY(BlueprintReadWrite, Replicated, EditDefaultsOnly, Category = "AGR|Setup")
	float TurnStartAngle = 90.0f;

	UPROPERTY(BlueprintReadWrite, Replicated, EditDefaultsOnly, Category = "AGR|Setup")
	float TurnStopTolerance = 1.0f;

	UPROPERTY(BlueprintReadWrite, Replicated, EditDefaultsOnly, Category = "AGR|Setup")
	EAimOffsets AimOffsetType = EAimOffsets::NONE;

	UPROPERTY(BlueprintReadWrite, Replicated, EditDefaultsOnly, Category = "AGR|Setup")
	EAimOffsetClamp AimOffsetBehavior = EAimOffsetClamp::Nearest;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AGR|Setup")
	float AimClamp = 135.0f;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AGR|Setup")
	bool CameraBased;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AGR|Setup")
	FName AimSocketName = "hand_r";

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AGR|Setup")
	FName LookAtSocketName = "Spine_03";

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AGR|Debug")
	TEnumAsByte <ECollisionChannel> TraceChannel = ECollisionChannel::ECC_Visibility;

	UPROPERTY(BlueprintReadOnly, Category = "AGR|Components")
	ACharacter* OwningCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "AGR|Components")
	UCharacterMovementComponent* OwnerMovementComponent;

	UPROPERTY(BlueprintAssignable, Category = "AGR|FirstPerson")
	FOnFirstPerson OnFirstPerson;

	#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AGR|Debug")
	bool bDebug;
	#endif

private:
	#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UUserWidget* DebugWidget;

	UPROPERTY()
	UAGRDebuggerController* DebuggerController;

	//Color of the line drawn to display when the starting point is LookAtSocket.
	UPROPERTY(EditDefaultsOnly, Category = "AGR|Debug")
	FColor LookAtLineColor;

	//Color of the line drawn to display when the starting point is AimSocket.
	UPROPERTY(EditDefaultsOnly, Category = "AGR|Debug")
	FColor AimLineColor;

	UPROPERTY(EditDefaultsOnly, Category = "AGR|Debug")
	bool bLinePersists;

	UPROPERTY(EditDefaultsOnly, Category = "AGR|Debug")
	float LineThickness;

	UPROPERTY(EditDefaultsOnly, Category = "AGR|Debug")
	float LineLifetime;
	#endif

public:
	UAGRAnimMasterComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AGR|Poses")
	void SetupBasePose(FGameplayTag InBasePose);

	UFUNCTION(BlueprintCallable, Category = "AGR|Poses")
    void SetupOverlayPose(FGameplayTag InOverlayPose);

	UFUNCTION(BlueprintCallable, Category = "AGR|FirstPerson")
	void SetupFpp(bool bInFirstPerson);

	UFUNCTION(BlueprintCallable, Category = "AGR|Rotation")
	void SetupRotation(
	    const ERotationMethod InRotationMethod = ERotationMethod::NONE,
	    const float InRotationSpeed = 360.0f,
	    const float InTurnStartAngle = 90.0f,
	    const float InTurnStopTolerance = 5.0f);

	UFUNCTION(BlueprintCallable, Category = "AGR|AimOffset")
	void SetupAimOffset(
		const EAimOffsets InAimOffsetType = EAimOffsets::NONE,
	    const EAimOffsetClamp InAimBehavior = EAimOffsetClamp::Nearest,
	    const float InAimClamp = 90.0f,
	    const bool InCameraBased = true,
	    const FName InAimSocketName = "hand_r",
		const FName InLookAtSocketName = "head");

	UFUNCTION(BlueprintCallable, Category = "AGR|AnimTags")
	void AddTag(FGameplayTag InTag);

	UFUNCTION(BlueprintCallable, Category = "AGR|AnimTags")
    bool RemoveTag(FGameplayTag InTag);

	UFUNCTION(BlueprintCallable, Category = "AGR|Tick")
	void AimTick();

	UFUNCTION(BlueprintCallable, Category = "AGR|Tick")
	void TurnInPlaceTick();

protected:
	virtual void BeginPlay() override;

private:
	//Function that recasts the result of GetOwner to character and sets a few references for the movement component and the owner itself.
	void RecastOwner();

	void HandleRotationSpeedChange();

	void HandleRotationMethodChange();

	void LookAtIfPlayerControlled();

	void LookAtWithoutCamera();

	UFUNCTION()
	void OnRep_RotationMethod();

	UFUNCTION()
	void OnRep_RotationSpeed();

	UFUNCTION(Server, Reliable)
	void ServerSetBasePose(const FGameplayTag InBasePose);

	UFUNCTION(Server, Reliable)
    void ServerSetOverlayPose(const FGameplayTag InOverlayPose);

	UFUNCTION(Server, Reliable)
	void ServerSetRotation(
		const ERotationMethod InRotationMethod,
		const float InRotationSpeed,
		const float InTurnStartAngle,
		const float InTurnStopTolerance);

	UFUNCTION(Server, Unreliable)
	void ServerSetupAimOffset(const EAimOffsets InAimOffsetType, const EAimOffsetClamp InAimBehavior);

	UFUNCTION(Server, Unreliable)
	void ServerSetAimOffset(const FRotator InAimOffset);

	UFUNCTION(NetMulticast, Unreliable)
	void NetMutiSetAimOffset(const FRotator InAimOffset);

	UFUNCTION(Server, Unreliable)
	void ServerSetLookAt(const FVector LookAt);

	UFUNCTION(NetMulticast, Unreliable)
	void NetMutiSetLookAt(const FVector LookAt);

	#if WITH_EDITOR
	/** Setup and register Gameplay Debug Widget. Handles input binding for activation of the widget */
	void SetupGameplayDebug();
	#endif
};
