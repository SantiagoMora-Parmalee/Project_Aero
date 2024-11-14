// Fill out your copyright notice in the Description page of Project Settings.


#include "ExtendedCharacter.h"
#include "ExtendedMovementComponent.h"

// Sets default values
AExtendedCharacter::AExtendedCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UExtendedMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	EMC = Cast<UExtendedMovementComponent>(GetCharacterMovement());
}

void AExtendedCharacter::StartGlide()
{
	EMC->SetGliding(true);
}

void AExtendedCharacter::StopGlide()
{
	EMC->SetGliding(false);
}
