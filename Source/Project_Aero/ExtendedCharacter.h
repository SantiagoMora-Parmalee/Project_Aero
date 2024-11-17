// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ExtendedCharacter.generated.h"

class UExtendedMovementComponent;  // Forward declaration of UExtendedMovementComponent class

/**
 * AExtendedCharacter is a subclass of ACharacter that extends the default character functionality
 * with custom movement capabilities, such as gliding.
 */
UCLASS()
class PROJECT_AERO_API AExtendedCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	// The constructor also initializes the Extended Movement Component
	AExtendedCharacter(const FObjectInitializer& ObjectInitializer);

	/**
	 * Returns the Extended Movement Component (EMC) for this character.
	 * This component handles custom movement logic (e.g., gliding).
	 *
	 * @return Pointer to UExtendedMovementComponent
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|Extended")
	UExtendedMovementComponent* GetExtendedMovementComponent() const { return EMC; }

	/**
	 * Starts the gliding behavior for the character by setting the movement component's gliding flag to true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Extended")
	void StartGlide();

	/**
	 * Stops the gliding behavior for the character by setting the movement component's gliding flag to false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Extended")
	void StopGlide();

	/**
	 * This event is called when the character collides with something while gliding.
	 * It can be implemented in Blueprints to define custom behavior upon glide impact.
	 *
	 * @param Hit Contains the details of the collision impact.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnGlideImpact(const FHitResult& Hit);

protected:
	/** A reference to the custom movement component used by this character (handles gliding). */
	UPROPERTY()
	TObjectPtr<UExtendedMovementComponent> EMC{ nullptr };

private:
	// Private members can be added here if needed
};
