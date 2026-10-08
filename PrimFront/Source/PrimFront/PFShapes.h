#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UMaterialInterface;

/** The engine's built-in basic shapes (/Engine/BasicShapes). Each is 100cm across, pivot at its centre. */
enum class EPFShape : uint8
{
	Cube,
	Sphere,
	Cylinder, // axis along local Z
	Cone      // tip along local +Z
};

/** Helpers for building every visual in the project out of untextured primitives. Sizes are in cm. */
struct PRIMFRONT_API FPFShapes
{
	static UStaticMesh* GetMesh(EPFShape Shape);
	static UMaterialInterface* GetBaseMaterial();

	/** Gives the component its own dynamic instance of BasicShapeMaterial with a flat colour. */
	static void SetColor(UPrimitiveComponent* Comp, const FLinearColor& Color);

	/** Creates, attaches and registers a primitive at runtime. Size is the full extent of the shape in cm. */
	static UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, EPFShape Shape,
		const FVector& Size, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color,
		bool bCollision = false, bool bCastShadow = true);

	/** Creates, attaches and registers an empty scene component used as a pivot or joint. */
	static USceneComponent* AddJoint(AActor* Owner, USceneComponent* Parent, const FVector& Location,
		const FRotator& Rotation = FRotator::ZeroRotator);

	/** Rotation that points a component's local -Z axis along Dir (limbs hang along -Z). */
	static FQuat AlongDown(const FVector& Dir);

	/** Rotation that points a component's local +Z axis along Dir (cylinders used as beams). */
	static FQuat AlongUp(const FVector& Dir);

	/** Rotation/translation blend between two transforms (scale ignored). */
	static FTransform Blend(const FTransform& A, const FTransform& B, float Alpha);
};
