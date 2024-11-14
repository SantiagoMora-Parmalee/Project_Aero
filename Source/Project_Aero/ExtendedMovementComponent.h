// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ExtendedMovementComponent.generated.h"

class AExtendedCharacter;

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None UMETA(Hidden),
	CMOVE_Glide UMETA(DisplayName = "Gliding"),
	CMOVE_MAX UMETA(Hidden),
};

/**
 *
 */
UCLASS()
class PROJECT_AERO_API UExtendedMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	friend class FSavedMove_Extended;
public:

	UFUNCTION(BlueprintCallable, Category = " ExtendedMovement|Gliding")
	FORCEINLINE bool IsGliding() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Glide && UpdatedComponent; }

	void SetGliding(bool Value) { bWantsToGlide = Value; }

protected:
#pragma region Overrides
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void InitializeComponent() override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	void PhysGlide(float deltaTime, int32 Iterations);

#pragma region Gliding
	UPROPERTY(BlueprintReadOnly, replicatedUsing = OnRep_WantsToGlide, Category = " ExtendedMovement|Gliding")
	uint8 bWantsToGlide : 1;

	UPROPERTY(EditDefaultsOnly, Category = " ExtendedMovement|Gliding")
	float GlideDownwardInfluence{ 10.f };

	UPROPERTY(EditDefaultsOnly, Category = " ExtendedMovement|Gliding")
	float GlideForwardInfluence{ 2.f };

	UPROPERTY(EditDefaultsOnly, Category = " ExtendedMovement|Gliding")
	float GlideGravityInfluence{ 750.f };

	UPROPERTY(EditDefaultsOnly, Category = " ExtendedMovement|Gliding")
	float GlideFrictionFactor{ 0.1f };

	UFUNCTION()
	void OnRep_WantsToGlide();

#pragma region Generic
	UPROPERTY(Transient)
	TObjectPtr<AExtendedCharacter> ExtendedCharacterOwner{ nullptr };

private:

};

#pragma region SavedMove

class FSavedMove_Extended : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	// Resets all saved variables.
	virtual void Clear() override;

	// Store input commands in the compressed flags.
	virtual uint8 GetCompressedFlags() const override;

	// This is used to check whether or not two moves can be combined into one.
	// Basically you just check to make sure that the saved variables are the same.
	virtual bool CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const override;

	// Sets up the move before sending it to the server.
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;

	// Sets variables on character movement component before making a predictive correction.
	virtual void PrepMoveFor(ACharacter* Character) override;

private:
	uint8 SavedWantsToGlide : 1;
};

#pragma region Network Prediction

class FNetworkPredictionData_Client_Extended : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	// Constructor
	FNetworkPredictionData_Client_Extended(const UCharacterMovementComponent& ClientMovement);

	// brief Allocates a new copy of our custom saved move
	virtual FSavedMovePtr AllocateNewMove() override;
};
