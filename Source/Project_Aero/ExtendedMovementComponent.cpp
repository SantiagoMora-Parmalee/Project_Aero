// Copyright notice placeholder - fill out in the Project Settings

#include "ExtendedMovementComponent.h"
#include "ExtendedCharacter.h"
#include "GameFramework/PhysicsVolume.h"
#include "Net/UnrealNetwork.h"

// Function to handle replication of properties across the network
void UExtendedMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Replicates the bWantsToGlide property, notifying all clients when it changes
	DOREPLIFETIME_CONDITION_NOTIFY(UExtendedMovementComponent, bWantsToGlide, COND_SkipOwner, REPNOTIFY_Always)
}

// Initialize the movement component, particularly setting the owner character
void UExtendedMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	// Cast the owner of this component to an AExtendedCharacter for easier reference
	ExtendedCharacterOwner = Cast<AExtendedCharacter>(GetOwner());
}

// Returns client-side network prediction data specific to this movement component
FNetworkPredictionData_Client* UExtendedMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

		if (ClientPredictionData == nullptr)
		{
			// Create a new network prediction data object for the extended movement
			UExtendedMovementComponent* MutableThis = const_cast<UExtendedMovementComponent*>(this);
			MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Extended(*this);
			MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;  // Maximum distance for smooth updates
			MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f; // Distance beyond which updates are not smoothed
		}
	return ClientPredictionData;
}

// Update the character's state before movement (e.g., gliding state handling)
void UExtendedMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Skip updates for simulated proxies (clients)
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy) { return; }

	// If the character wants to glide and isn't already gliding, set the movement mode to CMOVE_Glide
	if (bWantsToGlide)
	{
		if (CustomMovementMode != CMOVE_Glide)
		{
			SetMovementMode(MOVE_Custom, CMOVE_Glide);
			bOrientRotationToMovement = false;  // Disable rotation to movement while gliding
		}
	}
	// If the character doesn't want to glide but is currently gliding, revert to falling state
	else if (!bWantsToGlide && CustomMovementMode == CMOVE_Glide)
	{
		SetMovementMode(MOVE_Falling);
		bOrientRotationToMovement = true;  // Enable rotation to movement when falling
	}
}

// Update movement state based on compressed flags (networked movement optimizations)
void UExtendedMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Check the custom move flags to update the gliding state
	bWantsToGlide = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

// Process landing behavior (e.g., checking if character landed on walkable surface)
void UExtendedMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit))
	{
		CharacterOwner->Landed(Hit);  // Notify character that it landed
	}

	// Check if the character is still falling or gliding
	if (IsFalling() || IsGliding())
	{
		// Ensure the character is correctly placed on the ground (i.e., not stuck in an infinite loop)
		if (GetGroundMovementMode() == MOVE_NavWalking)
		{
			const FVector TestLocation = GetActorFeetLocation();
			FNavLocation NavLocation;

			// Check if the character is standing on a valid navmesh
			const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
			if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
			{
				SetGroundMovementMode(MOVE_Walking);  // Switch to walking mode if no valid navmesh
			}
		}

		SetPostLandedPhysics(Hit);  // Handle physics after landing
	}

	// Call path following agent to process landing
	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		PFAgent->OnLanded();
	}

	StartNewPhysics(remainingTime, Iterations);  // Start a new physics iteration
}

// Set movement mode and physics after landing (e.g., swimming or walking)
void UExtendedMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	if (CharacterOwner)
	{
		if (CanEverSwim() && IsInWater())
		{
			SetMovementMode(MOVE_Swimming);  // Switch to swimming if the character is in water
		}
		else
		{
			bOrientRotationToMovement = true;
			const FVector PreImpactAccel = Acceleration + (IsFalling() ? -GetGravityDirection() * GetGravityZ() : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

			// Set the appropriate movement mode after landing
			if (DefaultLandMovementMode == MOVE_Walking ||
				DefaultLandMovementMode == MOVE_NavWalking ||
				DefaultLandMovementMode == MOVE_Falling)
			{
				SetMovementMode(GetGroundMovementMode());
			}
			else
			{
				SetDefaultMovementMode();
			}

			ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);  // Apply forces after impact
		}
	}
}

// Custom physics handling for movement (e.g., for gliding)
void UExtendedMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	// Skip updates for simulated proxies
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy) { return; }

	// Handle custom movement modes (e.g., gliding)
	switch (CustomMovementMode)
	{
	case CMOVE_Glide:
		PhysGlide(deltaTime, Iterations);  // Handle gliding physics
		break;
	default:
		break;
	}
}

// Physics implementation for gliding
void UExtendedMovementComponent::PhysGlide(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;  // Skip if deltaTime is too small
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!CharacterOwner) { return; }

	FVector OldVelocity = Velocity;
	FVector CurrentDirection = OldVelocity;
	float CurrentSpeed = OldVelocity.Length();

	// Calculate movement direction based on camera and actor's forward/backward/right direction
	FVector CameraForward = CharacterOwner->GetControlRotation().Vector();
	FVector ActorForward = CharacterOwner->GetActorForwardVector();
	FVector ActorRight = CharacterOwner->GetActorRightVector();
	FVector ActorUp = CharacterOwner->GetActorUpVector();

	// Compute the target velocity for gliding based on camera and actor orientation
	FVector TargetVelocity = CameraForward * CurrentSpeed;

	// Apply downward force and forward influence for gliding
	FVector GravityVelocity = (GetGravityDirection() * GetGravityZ() * GlideDownwardInfluence * -1) + OldVelocity + (ActorForward * GlideForwardInfluence);
	GravityVelocity.Normalize();
	GravityVelocity *= GlideGravityInfluence;

	// Adjust velocity based on camera orientation
	float DP_Up = CameraForward | ActorUp;
	if (DP_Up < 0.f)
	{
		GravityVelocity += CameraForward * GetGravityZ() * DP_Up;
	}

	// Adjust the character's velocity based on gliding behavior
	FVector VelocityAdjustment = TargetVelocity + GravityVelocity - Velocity;
	Velocity += VelocityAdjustment * deltaTime;

	// Apply friction and calculate new velocity based on input and gliding friction
	Acceleration = FVector::ZeroVector;  // No input during gliding
	const float Friction = GlideFrictionFactor * GetPhysicsVolume()->FluidFriction;
	CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());

	ApplyRootMotionToVelocity(deltaTime);  // Apply any root motion if necessary

	Iterations++;  // Increment the iteration count
	bJustTeleported = false;  // Reset teleportation flag

	// Handle collisions and impact while gliding
	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FQuat DesiredRotation = FRotationMatrix::MakeFromZX(ActorUp, Velocity.GetSafeNormal()).ToQuat();
	FQuat CurrentRotation = UpdatedComponent->GetComponentRotation().Quaternion();
	FQuat NewRotation = FMath::QInterpConstantTo(CurrentRotation, DesiredRotation, deltaTime, 50.f);
	FHitResult Hit(1.f);

	SafeMoveUpdatedComponent(Adjusted, NewRotation, true, Hit);  // Move the character and handle collisions

	// Handle landing if the character hits a walkable surface
	if (Hit.Time < 1.f)
	{
		if (IsWalkable(Hit))


