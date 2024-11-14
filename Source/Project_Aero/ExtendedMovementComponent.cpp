// Fill out your copyright notice in the Description page of Project Settings.


#include "ExtendedMovementComponent.h"
#include "ExtendedCharacter.h"
#include "GameFramework/PhysicsVolume.h"
#include "Net/UnrealNetwork.h"

void UExtendedMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UExtendedMovementComponent, bWantsToGlide, COND_SkipOwner, REPNOTIFY_Always)
}

void UExtendedMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	ExtendedCharacterOwner = Cast<AExtendedCharacter>(GetOwner());
}

FNetworkPredictionData_Client* UExtendedMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

		if (ClientPredictionData == nullptr)
		{
			// const cast is bad, but this is the only way
			UExtendedMovementComponent* MutableThis = const_cast<UExtendedMovementComponent*>(this);
			MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Extended(*this);
			MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
			MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
		}
	return ClientPredictionData;
}

void UExtendedMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy) { return; }

	// This needs to be the same as in OnRep_WantsToGlide
	if (bWantsToGlide)
	{
		if (CustomMovementMode != CMOVE_Glide)
		{
			SetMovementMode(MOVE_Custom, CMOVE_Glide);
			bOrientRotationToMovement = false;
		}
	}
	else if (!bWantsToGlide && CustomMovementMode == CMOVE_Glide)
	{
		SetMovementMode(MOVE_Falling);
		bOrientRotationToMovement = true;
	}
}

void UExtendedMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	/*  There are 4 custom move flags for us to use. Below is what each is currently being used for:
		FLAG_Custom_0		= 0x10, // Gliding
		FLAG_Custom_1		= 0x20, // Unused
		FLAG_Custom_2		= 0x40, // Unused
		FLAG_Custom_3		= 0x80, // Unused
	*/

	// Read the values from the compressed flags
	bWantsToGlide = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UExtendedMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit))
	{
		CharacterOwner->Landed(Hit);
	}

	if (IsFalling() || IsGliding())
	{
		if (GetGroundMovementMode() == MOVE_NavWalking)
		{
			// verify navmesh projection and current floor
			// otherwise movement will be stuck in infinite loop:
			// navwalking -> (no navmesh) -> falling -> (standing on something) -> navwalking -> ....

			const FVector TestLocation = GetActorFeetLocation();
			FNavLocation NavLocation;

			const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
			if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
			{
				SetGroundMovementMode(MOVE_Walking);
			}
		}

		SetPostLandedPhysics(Hit);
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		PFAgent->OnLanded();
	}

	StartNewPhysics(remainingTime, Iterations);
}

void UExtendedMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	if (CharacterOwner)
	{
		if (CanEverSwim() && IsInWater())
		{
			SetMovementMode(MOVE_Swimming);
		}
		else
		{
			bOrientRotationToMovement = true;
			const FVector PreImpactAccel = Acceleration + (IsFalling() ? -GetGravityDirection() * GetGravityZ() : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

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
			ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);
		}
	}
}

void UExtendedMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy) { return; }


	switch (CustomMovementMode)
	{
	case CMOVE_Glide:
		PhysGlide(deltaTime, Iterations);
		break;
	default:
		break;
	}
}

void UExtendedMovementComponent::PhysGlide(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!CharacterOwner) { return; }

	FVector OldVelocity = Velocity;
	FVector CurrentDirection = OldVelocity;
	float CurrentSpeed = OldVelocity.Length();

	FVector CameraForward = CharacterOwner->GetControlRotation().Vector();
	FVector ActorForward = CharacterOwner->GetActorForwardVector();
	FVector ActorRight = CharacterOwner->GetActorRightVector();
	FVector ActorUp = CharacterOwner->GetActorUpVector();

	// DP_X > 0 looking in that direction
	float DP_Up = CameraForward | ActorUp;
	float DP_Right = CameraForward | ActorRight;

	// Velocity in the direction we want to move
	FVector TargetVelocity = CameraForward * CurrentSpeed;

	FVector GravityVelocity = (GetGravityDirection() * GetGravityZ() * GlideDownwardInfluence * -1) + OldVelocity + (ActorForward * GlideForwardInfluence);
	GravityVelocity.Normalize();
	GravityVelocity *= GlideGravityInfluence;

	if (DP_Up < 0.f)
	{
		GravityVelocity += CameraForward * GetGravityZ() * DP_Up;
	}

	FVector VelocityAdjustment = TargetVelocity + GravityVelocity - Velocity;
	Velocity += VelocityAdjustment * deltaTime;


	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		if (bCheatFlying && Acceleration.IsZero())
		{
			Velocity = FVector::ZeroVector;
		}

		// How input is handled, but if there's going to be any input is unclear
		//float AccelerationStateModifier = 1.f;
		//Acceleration = Acceleration.ProjectOnTo(UpdatedComponent->GetRightVector().GetSafeNormal2D()) * AccelerationStateModifier;
		//Acceleration.X = 0.f;
		Acceleration = FVector::ZeroVector;
		const float Friction = GlideFrictionFactor * GetPhysicsVolume()->FluidFriction;
		CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);

	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FQuat DesiredRotation = FRotationMatrix::MakeFromZX(ActorUp, Velocity.GetSafeNormal()).ToQuat();
	FQuat CurrentRotation = UpdatedComponent->GetComponentRotation().Quaternion();
	FQuat NewRotation = FMath::QInterpConstantTo(CurrentRotation, DesiredRotation, deltaTime, 50.f);
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, NewRotation, true, Hit);

	if (Hit.Time < 1.f)
	{
		if (IsWalkable(Hit))
		{
			ProcessLanded(Hit, deltaTime, Iterations);
			return;
		}
		else
		{
			ExtendedCharacterOwner->OnGlideImpact(Hit);
		}

		const FVector GravDir = GetGravityDirection();
		const FVector VelDir = Velocity.GetSafeNormal();
		const float UpDown = GravDir | VelDir;

		bool bSteppedUp = false;
		if ((FMath::Abs(Hit.ImpactNormal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
		{
			float stepZ = UpdatedComponent->GetComponentLocation().Z;
			bSteppedUp = StepUp(GravDir, Adjusted * (1.f - Hit.Time), Hit);
			if (bSteppedUp)
			{
				OldLocation.Z = UpdatedComponent->GetComponentLocation().Z + (OldLocation.Z - stepZ);
			}
		}

		if (!bSteppedUp)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}
}

void UExtendedMovementComponent::OnRep_WantsToGlide()
{
	if (bWantsToGlide)
	{
		if (CustomMovementMode != CMOVE_Glide)
		{
			SetMovementMode(MOVE_Custom, CMOVE_Glide);
			bOrientRotationToMovement = false;
		}
	}
	else if (!bWantsToGlide && CustomMovementMode == CMOVE_Glide)
	{
		SetMovementMode(MOVE_Falling);
		bOrientRotationToMovement = true;
	}
}

void FSavedMove_Extended::Clear()
{
	Super::Clear();
	SavedWantsToGlide = 0;
}

uint8 FSavedMove_Extended::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	/* There are 4 custom move flags for us to use. Below is what each is currently being used for:
	FLAG_Custom_0		= 0x10, // Gliding
	FLAG_Custom_1		= 0x20, // Unused
	FLAG_Custom_2		= 0x40, // Unused
	FLAG_Custom_3		= 0x80, // Unused
	*/

	// Write to the compressed flags
	if (SavedWantsToGlide)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

bool FSavedMove_Extended::CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_Extended* NewMove = static_cast<const FSavedMove_Extended*>(NewMovePtr.Get());

	// check to see if engine can combine moves
	if (SavedWantsToGlide != NewMove->SavedWantsToGlide) { return false; }

	return Super::CanCombineWith(NewMovePtr, Character, MaxDelta);
}

void FSavedMove_Extended::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	UExtendedMovementComponent* MoveComp = static_cast<UExtendedMovementComponent*>(Character->GetCharacterMovement());
	if (MoveComp)
	{
		// Copy values into the saved move
		SavedWantsToGlide = MoveComp->bWantsToGlide;
	}
}

void FSavedMove_Extended::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UExtendedMovementComponent* MoveComp = static_cast<UExtendedMovementComponent*>(Character->GetCharacterMovement());
	if (MoveComp)
	{
		// Copy values out of the saved move
		MoveComp->bWantsToGlide = SavedWantsToGlide;
	}
}

FNetworkPredictionData_Client_Extended::FNetworkPredictionData_Client_Extended(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
	// Nothing is needed here
}

FSavedMovePtr FNetworkPredictionData_Client_Extended::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Extended());
}
