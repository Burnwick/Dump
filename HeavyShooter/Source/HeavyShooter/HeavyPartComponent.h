#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "HeavyPartComponent.generated.h"

class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EHeavyShape : uint8
{
	Cube,
	Cylinder,
	Sphere,
	Cone
};

/**
 * A coloured engine primitive (cube / cylinder / sphere / cone from /Engine/BasicShapes).
 * The trooper, the shotgun, the dummies and the range are all assembled from these,
 * so the project runs without any imported art. Swap any part for a real mesh later.
 */
UCLASS(ClassGroup = (Heavy), meta = (BlueprintSpawnableComponent))
class HEAVYSHOOTER_API UHeavyPartComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UHeavyPartComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy")
	FLinearColor PartColor = FLinearColor(0.5f, 0.5f, 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Heavy")
	void SetPartColor(const FLinearColor& NewColor);

	static UStaticMesh* GetShapeMesh(EHeavyShape Shape);

	/** Constructor-only helper. SizeCm is the final size in centimetres (the engine shapes are 100 cm). */
	static UHeavyPartComponent* CreatePart(AActor* Owner, FName Name, USceneComponent* Parent, EHeavyShape Shape,
		const FVector& Location, const FVector& SizeCm, const FRotator& Rotation, const FLinearColor& Color);

	/** Constructor-only helper: a cylinder whose axis runs along the parent's X axis (barrels, tubes). */
	static UHeavyPartComponent* CreateRodX(AActor* Owner, FName Name, USceneComponent* Parent,
		const FVector& Center, float Diameter, float Length, const FLinearColor& Color);

	/** Runtime helper: creates, attaches and registers a part immediately. */
	static UHeavyPartComponent* SpawnPart(AActor* Owner, USceneComponent* Parent, EHeavyShape Shape,
		const FVector& Location, const FVector& SizeCm, const FRotator& Rotation, const FLinearColor& Color, bool bWithCollision);

protected:
	virtual void OnRegister() override;

private:
	void ApplyColor();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColorMaterial;
};
