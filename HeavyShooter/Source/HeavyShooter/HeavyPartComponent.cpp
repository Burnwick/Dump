#include "HeavyPartComponent.h"

#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// /Engine/BasicShapes/BasicShapeMaterial exposes a vector parameter called "Color".
	const FName ColorParameterName(TEXT("Color"));

	const TCHAR* ShapePath(EHeavyShape Shape)
	{
		switch (Shape)
		{
		case EHeavyShape::Cylinder: return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		case EHeavyShape::Sphere:   return TEXT("/Engine/BasicShapes/Sphere.Sphere");
		case EHeavyShape::Cone:     return TEXT("/Engine/BasicShapes/Cone.Cone");
		case EHeavyShape::Cube:
		default:                    return TEXT("/Engine/BasicShapes/Cube.Cube");
		}
	}
}

UHeavyPartComponent::UHeavyPartComponent()
{
	// Purely visual by default; owners opt in to collision where it matters.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	bReceivesDecals = false;
}

UStaticMesh* UHeavyPartComponent::GetShapeMesh(EHeavyShape Shape)
{
	static TWeakObjectPtr<UStaticMesh> Cache[4];
	TWeakObjectPtr<UStaticMesh>& Slot = Cache[static_cast<uint8>(Shape) & 3];
	if (!Slot.IsValid())
	{
		Slot = LoadObject<UStaticMesh>(nullptr, ShapePath(Shape));
	}
	return Slot.Get();
}

UHeavyPartComponent* UHeavyPartComponent::CreatePart(AActor* Owner, FName Name, USceneComponent* Parent, EHeavyShape Shape,
	const FVector& Location, const FVector& SizeCm, const FRotator& Rotation, const FLinearColor& Color)
{
	UHeavyPartComponent* Part = Owner->CreateDefaultSubobject<UHeavyPartComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(GetShapeMesh(Shape));
	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(SizeCm / 100.f);
	Part->PartColor = Color;
	return Part;
}

UHeavyPartComponent* UHeavyPartComponent::CreateRodX(AActor* Owner, FName Name, USceneComponent* Parent,
	const FVector& Center, float Diameter, float Length, const FLinearColor& Color)
{
	// Engine cylinders stand on Z; pitch 90 lays them along X.
	return CreatePart(Owner, Name, Parent, EHeavyShape::Cylinder, Center,
		FVector(Diameter, Diameter, Length), FRotator(90.f, 0.f, 0.f), Color);
}

UHeavyPartComponent* UHeavyPartComponent::SpawnPart(AActor* Owner, USceneComponent* Parent, EHeavyShape Shape,
	const FVector& Location, const FVector& SizeCm, const FRotator& Rotation, const FLinearColor& Color, bool bWithCollision)
{
	UHeavyPartComponent* Part = NewObject<UHeavyPartComponent>(Owner);
	Part->SetStaticMesh(GetShapeMesh(Shape));
	Part->SetupAttachment(Parent);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(SizeCm / 100.f);
	Part->PartColor = Color;
	if (bWithCollision)
	{
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Part->SetCanEverAffectNavigation(true);
	}
	Part->RegisterComponent();
	Owner->AddInstanceComponent(Part);
	return Part;
}

void UHeavyPartComponent::OnRegister()
{
	Super::OnRegister();
	ApplyColor();
}

void UHeavyPartComponent::SetPartColor(const FLinearColor& NewColor)
{
	PartColor = NewColor;
	ApplyColor();
}

void UHeavyPartComponent::ApplyColor()
{
	if (!ColorMaterial || ColorMaterial->GetOuter() != this)
	{
		// Parent off the mesh's own material (BasicShapeMaterial), not a previous instance.
		UMaterialInterface* Base = GetMaterial(0);
		if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(Base))
		{
			Base = ExistingMID->Parent;
		}
		if (!Base)
		{
			return;
		}
		// Transient so placed actors never save a material instance into the level.
		ColorMaterial = UMaterialInstanceDynamic::Create(Base, this);
		ColorMaterial->SetFlags(RF_Transient);
		SetMaterial(0, ColorMaterial);
	}
	ColorMaterial->SetVectorParameterValue(ColorParameterName, PartColor);
}
