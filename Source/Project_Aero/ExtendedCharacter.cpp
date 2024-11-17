// Copyright notice placeholder - fill out in the Project Settings

#include "ExtendedCharacter.h"
#include "ExtendedMovementComponent.h"

// Constructor for the AExtendedCharacter class
// Initializes the character and sets the default subobject for movement component to UExtendedMovementComponent
AExtendedCharacter::AExtendedCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UExtendedMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    // Enable ticking (the character's Update() function will be called every frame)
    // Set this to false to improve performance if tick functionality is not needed.
    PrimaryActorTick.bCanEverTick = true;

    // Retrieve the character's movement component and cast it to the custom UExtendedMovementComponent
    EMC = Cast<UExtendedMovementComponent>(GetCharacterMovement());
}

// Starts gliding by setting the gliding state to true in the movement component
void AExtendedCharacter::StartGlide()
{
    EMC->SetGliding(true);  // Set the gliding state in the movement component
}

// Stops gliding by setting the gliding state to false in the movement component
void AExtendedCharacter::StopGlide()
{
    EMC->SetGliding(false);  // Set the gliding state to false, stopping the glide
}
