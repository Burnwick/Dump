#include "PFShapes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UStaticMesh* FPFShapes::GetMesh(EPFShape Shape)
{
	const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
	switch (Shape)
	{
	case EPFShape::Sphere:   Path = TEXT("/Engine/BasicShapes/Sphere.Sphere"); break;
	case EPFShape::Cylinder: Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break;
	case EPFShape::Cone:     Path = TEXT("/Engine/BasicShapes/Cone.Cone"); break;
	default: break;
	}
	return LoadObject<UStaticMesh>(nullptr, Path);
}

UMaterialInterface* FPFShapes::GetBaseMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

void FPFShapes::SetColor(UPrimitiveComponent* Comp, const FLinearColor& Color)
{
	if (!Comp)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
	if (!MID)
	{
		if (UMaterialInterface* Base = GetBaseMaterial())
		{
			MID = UMaterialInstanceDynamic::Create(Base, Comp);
			Comp->SetMaterial(0, MID);
		}
	}
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

UStaticMeshComponent* FPFShapes::AddPart(AActor* Owner, USceneComponent* Parent, EPFShape Shape,
	const FVector& Size, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color,
	bool bCollision, bool bCastShadow)
{
	check(Owner);

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(GetMesh(Shape));
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeRotation(Rotation);
	Comp->SetRelativeScale3D(Size / 100.f);
	Comp->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCastShadow(bCastShadow);
	Comp->SetCanEverAffectNavigation(bCollision);

	if (Parent)
	{
		Comp->SetupAttachment(Parent);
	}
	else if (!Owner->GetRootComponent())
	{
		Owner->SetRootComponent(Comp);
	}

	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);
	SetColor(Comp, Color);
	return Comp;
}

USceneComponent* FPFShapes::AddJoint(AActor* Owner, USceneComponent* Parent, const FVector& Location, const FRotator& Rotation)
{
	check(Owner);

	USceneComponent* Comp = NewObject<USceneComponent>(Owner);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetRelativeLocationAndRotation(Location, Rotation);
	if (Parent)
	{
		Comp->SetupAttachment(Parent);
	}
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);
	return Comp;
}

FQuat FPFShapes::AlongDown(const FVector& Dir)
{
	const FVector N = Dir.GetSafeNormal();
	return N.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(FVector(0.0, 0.0, -1.0), N);
}

FQuat FPFShapes::AlongUp(const FVector& Dir)
{
	const FVector N = Dir.GetSafeNormal();
	return N.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(FVector(0.0, 0.0, 1.0), N);
}

FTransform FPFShapes::Blend(const FTransform& A, const FTransform& B, float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.f, 1.f);
	return FTransform(FQuat::Slerp(A.GetRotation(), B.GetRotation(), T), FMath::Lerp(A.GetLocation(), B.GetLocation(), T));
}
