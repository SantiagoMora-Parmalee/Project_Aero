// Copyright notice placeholder - fill out in the Project Settings

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ExtendedMovementComponent.generated.h"

// Forward declaration of the AExtendedCharacter class.
class AExtendedCharacter;

// Enum for custom movement modes used in the movement component.
UENUM(BlueprintType)
enum ECustomMovementMode
{
	// No custom movement mode
	CMOVE_None UMETA(Hidden),
	// Represents gliding mode
	CMOVE_Glide UMETA(DisplayName = "Gliding"),
	// Maximum enum value (hidden, used internally)
	CMOVE_MAX UMETA(Hidden),
};

/**
 * UExtendedMovementComponent extends the base UCharacterMovementComponent
 * to support custom movement features like gliding.
 */
UCLASS()
class PROJECT_AERO_API UExtendedMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	friend class FSavedMove_Extended;  // Friend class declaration for network prediction data

public:
	// Function to check if the character is currently gliding
	// Returns true if the character is in the gliding movement mode
	UFUNCTION(BlueprintCallable, Category = "ExtendedMovement|Gliding")
	FORCEINLINE bool IsGliding() const {
		return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Glide && UpdatedComponent;
	}

	// Sets whether the character wants to glide or not.
	// This value is replicated across the network.
	void SetGliding(bool Value) {
		bWantsToGlide = Value;
	}

protected:
#pragma region Overrides

	// Replication of properties for networking
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Initialize the movement component
	virtual void InitializeComponent() override;

	// Client-side network prediction data setup
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	// Updates character state before processing movement
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	// Process flags from compressed network data
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	// Handle landing behavior (post-movement)
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	// Set physics properties after landing
	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

	// Custom physics update, called during the movement update cycle
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	// Gliding-specific physics update
	void PhysGlide(float deltaTime, int32 Iterations);

#pragma region Gliding

	// Boolean flag for whether the character wants to glide, replicated across network
	UPROPERTY(BlueprintReadOnly, replicatedUsing = OnRep_WantsToGlide, Category = "ExtendedMovement|Gliding")
	uint8 bWantsToGlide : 1;

	// Downward force applied when gliding, affecting the vertical movement
	UPROPERTY(EditDefaultsOnly, Category = "ExtendedMovement|Gliding")
	float GlideDownwardInfluence{ 10.f };

	// Forward force applied when gliding, affecting horizontal movement
	UPROPERTY(EditDefaultsOnly, Category = "ExtendedMovement|Gliding")
	float GlideForwardInfluence{ 2.f };

	// Gravity influence when gliding, modifying gravitational pull during glide
	UPROPERTY(EditDefaultsOnly, Category = "ExtendedMovement|Gliding")
	float GlideGravityInfluence{ 750.f };

	// Friction factor that affects gliding movement speed
	UPROPERTY(EditDefaultsOnly, Category = "ExtendedMovement|Gliding")
	float GlideFrictionFactor{ 0.1f };

	// Function called when bWantsToGlide is updated (replicated across network)
	UFUNCTION()
	void OnRep_WantsToGlide();

#pragma region Generic

	// Pointer to the owner character of this movement component (the character using this movement logic)
	UPROPERTY(Transient)
	TObjectPtr<AExtendedCharacter> ExtendedCharacterOwner{ nullptr };

private:
	// Private member variables can be added here if needed

};

#pragma region SavedMove

// Custom saved move class for extended movement, used for network prediction
class FSavedMove_Extended : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	// Resets all saved move variables
	virtual void Clear() override;

	// Compress input commands into flags (network optimization)
	virtual uint8 GetCompressedFlags() const override;

	// Determines if this move can be combined with another move (used in network prediction)
	virtual bool CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const override;

	// Set up the move before sending to the server
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;

	// Set movement component variables before applying a predictive correction
	virtual void PrepMoveFor(ACharacter* Character) override;

private:
	// Saved state of whether the character wants to glide or not for prediction purposes
	uint8 SavedWantsToGlide : 1;
};

#pragma region Network Prediction

// Custom network prediction data for the extended movement component
class FNetworkPredictionData_Client_Extended : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	// Constructor to initialize network prediction data for the extended movement component
	FNetworkPredictionData_Client_Extended(const UCharacterMovementComponent& ClientMovement);

	// Allocates a new saved move (used in network prediction)
	virtual FSavedMovePtr AllocateNewMove() override;
};
