#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFPhysicsProp.generated.h"

class UStaticMeshComponent;

/** A simulated crate for knocking around with pellets and melee. Scale the actor to resize it (1 = 100cm cube). */
UCLASS()
class PRIMFRONT_API APFPhysicsProp : public AActor
{
	GENERATED_BODY()

public:
	APFPhysicsProp();

	virtual void PostInitializeComponents() override;

	void SetColor(const FLinearColor& NewColor);

	UPROPERTY(EditAnywhere, Category = "Prop")
	FLinearColor Color = FLinearColor(0.45f, 0.3f, 0.12f);

	UPROPERTY(EditAnywhere, Category = "Prop")
	float MassKg = 25.f;

private:
	UPROPERTY(VisibleAnywhere, Category = "Prop")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
