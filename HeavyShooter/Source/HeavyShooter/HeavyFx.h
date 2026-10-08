#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavyPartComponent.h"
#include "HeavyFx.generated.h"

class UBoxComponent;

/**
 * A short-lived, collision-free primitive used for tracers, impact sparks and dust.
 * Scales from its spawn scale to EndScale over its lifetime, optionally flying ballistically.
 */
UCLASS(NotPlaceable)
class HEAVYSHOOTER_API AHeavyTransientFx : public AActor
{
	GENERATED_BODY()

public:
	AHeavyTransientFx();

	virtual void Tick(float DeltaSeconds) override;

	static AHeavyTransientFx* Spawn(UWorld* World, EHeavyShape Shape, const FTransform& Transform, const FLinearColor& Color,
		float Lifetime, const FVector& EndScale, const FVector& Velocity = FVector::ZeroVector, float Gravity = 0.f);

private:
	UPROPERTY(VisibleAnywhere, Category = "Fx")
	TObjectPtr<UHeavyPartComponent> Mesh;

	FVector StartScale = FVector::OneVector;
	FVector EndScale = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Gravity = 0.f;
	float Lifetime = 0.1f;
	float Age = 0.f;
};

/** A spent shotgun shell kicked out of the ejection port. Real physics, despawns after a few seconds. */
UCLASS(NotPlaceable)
class HEAVYSHOOTER_API AHeavyShellCasing : public AActor
{
	GENERATED_BODY()

public:
	AHeavyShellCasing();

	void Launch(const FVector& LinearVelocity, const FVector& AngularVelocityDegrees);

private:
	UPROPERTY(VisibleAnywhere, Category = "Shell")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Shell")
	TObjectPtr<UHeavyPartComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category = "Shell")
	TObjectPtr<UHeavyPartComponent> Base;
};
