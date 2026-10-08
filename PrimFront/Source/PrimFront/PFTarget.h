#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFShapes.h"
#include "PFTarget.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Pop-up training dummy made of primitives. Local +X faces the shooter.
 * Takes damage (head counts double), falls backwards when it runs out of health and pops back up.
 */
UCLASS()
class PRIMFRONT_API APFTarget : public AActor
{
	GENERATED_BODY()

public:
	APFTarget();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	bool IsDown() const { return bDown; }
	bool IsHeadComponent(const UPrimitiveComponent* Comp) const { return Comp && Comp == Head; }

	/** Stands the target back up at full health. */
	void ResetTarget();

	/** Makes the target sweep side to side along its local Y axis. */
	void SetMovement(float Distance, float Speed);

	UPROPERTY(EditAnywhere, Category = "Target")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Target")
	float HeadshotMultiplier = 2.f;

	/** Seconds the target stays down before popping back up. */
	UPROPERTY(EditAnywhere, Category = "Target")
	float ResetDelay = 3.f;

	/** Half-width of the side to side sweep in cm (0 = static). */
	UPROPERTY(EditAnywhere, Category = "Target")
	float MoveDistance = 0.f;

	/** Sweep speed in cycles per second. */
	UPROPERTY(EditAnywhere, Category = "Target")
	float MoveSpeed = 0.25f;

private:
	void BuildVisuals();
	void SetFlash(bool bOn);
	UStaticMeshComponent* AddColoredPart(USceneComponent* Parent, EPFShape Shape, const FVector& Size, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BasePlate;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> DamageText;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ColoredParts;

	TArray<FLinearColor> PartColors;

	FVector StartLocation = FVector::ZeroVector;
	float Health = 100.f;
	float FallAlpha = 0.f;
	float DownTimer = 0.f;
	float FlashTimer = 0.f;
	float TextTimer = 0.f;
	float DamageShown = 0.f;
	float MoveTime = 0.f;
	bool bDown = false;
	bool bBuilt = false;
};
