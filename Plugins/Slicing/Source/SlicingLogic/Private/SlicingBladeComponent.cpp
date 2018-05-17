// Copyright 2018, Institute for Artificial Intelligence - University of Bremen

#include "SlicingBladeComponent.h"
#include "SlicingTipComponent.h"

#include "DrawDebugHelpers.h"
#include "TransformCalculus.h"
#include "KismetProceduralMeshLibrary.h"

#include "Runtime/Engine/Classes/PhysicsEngine/PhysicsConstraintComponent.h"

#include "Engine/StaticMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"

// Called before BeginPlay()
void USlicingBladeComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

// Called when the game starts
void USlicingBladeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Check for the tip component to possibly abort the cutting
	TipComponent = FSlicingLogicModule::GetSlicingComponent<USlicingTipComponent>(SlicingObject);

	// Create the Physics Constraint
	ConstraintOne = NewObject<UPhysicsConstraintComponent>();
	ConstraintOne->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// Register the overlap events
	OnComponentBeginOverlap.AddDynamic(this, &USlicingBladeComponent::OnBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &USlicingBladeComponent::OnEndOverlap);
}

void USlicingBladeComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{	
	// This event is only important if the other object can actually be cut or if another cut hasn't already started
	if (!OtherComp->ComponentHasTag(TagCuttable) || bIsCurrentlyCutting)
	{
		return;
	}
	// If we are trying to start cutting with the tip, the slicing process should never start
	else if (TipComponent != NULL && OtherComp == TipComponent->CutComponent)
	{
		return;
	}

	// Collision should only be ignored with the currently cut object, not the objects around it
	SlicingObject->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Overlap);

	// If physics are on, the relative location and such will be seen relative to the world location
	RelativeLocationToCutComponent = OtherComp->GetComponentTransform().InverseTransformPosition(OverlappedComp->GetComponentLocation());
	RelativeRotationToCutComponent = OverlappedComp->GetComponentQuat() - OtherComp->GetComponentQuat();

	bIsCurrentlyCutting = true;
	CutComponent = OtherComp;
	CutComponent->SetNotifyRigidBodyCollision(true);

	SetUpConstrains(CutComponent);
}

void USlicingBladeComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// The slicing should only happen if you are actually in the cutting process and are in contact with the object
	// that is being cut
	if (!bIsCurrentlyCutting || OtherComp != CutComponent)
	{
		ResetState();
		return;
	}
	// If the SlicingObject is pulled out, the cutting should not be continued
	else if (TipComponent != NULL && OtherComp == TipComponent->CutComponent)
	{
		bIsCurrentlyCutting = false;
		ResetState();
		return;
	}

	// Abort the cutting if you stop cutting at the same point you started at
	FVector CutComponentPosition =
		UKismetMathLibrary::TransformLocation(CutComponent->GetComponentTransform(), RelativeLocationToCutComponent);
	if (OverlappedComp->OverlapComponent(CutComponentPosition, CutComponent->GetComponentQuat(), OverlappedComp->GetCollisionShape()))
	{
		// Collision should turn back to normal again
		SlicingObject->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Block);
		bIsCurrentlyCutting = false;

		ResetState();
		return;
	}

	// After everything is checked, the actual slicing happens here
	SliceComponent(OtherComp);

	ResetState();
}

void USlicingBladeComponent::SliceComponent(UPrimitiveComponent* CuttableComponent)
{
	TArray<FStaticMaterial> ComponentMaterials;
	UMaterialInterface* InsideCutMaterialInterface = nullptr;
	
	// In case the component is a StaticMeshComponent it needs to be converted into a ProceduralMeshComponent
	if (CuttableComponent->IsA(UStaticMeshComponent::StaticClass()))
	{
		CuttableComponent =	FSlicingLogicModule::ConvertStaticToProceduralMeshComponent(
			(UStaticMeshComponent*)CuttableComponent, ComponentMaterials
		);
	}

	// Check the Materials for the cut material
	for (int index = 0; index < ComponentMaterials.Num(); index++)
	{
		FStaticMaterial Material = ComponentMaterials[index];

		if (Material.MaterialSlotName == FName("InsideCutMaterial"))
		{
			// Found the needed material, do not need to search further
			InsideCutMaterialInterface = Material.MaterialInterface;
			break;
		}
	}
	
	UProceduralMeshComponent* OutputProceduralMesh;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		(UProceduralMeshComponent*)CuttableComponent,
		SlicingObject->GetSocketLocation(SocketBladeName),
		SlicingObject->GetUpVector(),
		true,
		OutputProceduralMesh,
		EProcMeshSliceCapOption::CreateNewSectionForCap,
		InsideCutMaterialInterface
	);

	OutputProceduralMesh->bGenerateOverlapEvents = true;
	OutputProceduralMesh->SetEnableGravity(true);
	OutputProceduralMesh->SetSimulatePhysics(true);
	OutputProceduralMesh->ComponentTags = CuttableComponent->ComponentTags;
	
	// Convert both seperated procedural meshes into static meshes for best compatibility
	FSlicingLogicModule::ConvertProceduralComponentToStaticMeshActor(OutputProceduralMesh);
	FSlicingLogicModule::ConvertProceduralComponentToStaticMeshActor((UProceduralMeshComponent*)CuttableComponent);

	// Delete old original static mesh
	CutComponent->GetOwner()->Destroy();
}

// Resets everything to the state the component was in before the cutting-process began
void USlicingBladeComponent::ResetState()
{
	bIsCurrentlyCutting = false;
	CutComponent = NULL;

	FlushPersistentDebugLines(this->GetWorld());

	// Resets the Constrains
	ConstraintOne->ConstraintInstance.SetLinearZLimit(ELinearConstraintMotion::LCM_Free, 1.f);
	ConstraintOne->ConstraintInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 1.f);
	ConstraintOne->ConstraintInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 1.f);
}

// Connects the given Component, normally the CuttableComponent, with either the Blade OR the Hand if it's welded.
void USlicingBladeComponent::SetUpConstrains(UPrimitiveComponent* CuttableComponent)
{
	ConstraintOne->ConstraintInstance.SetLinearBreakable(false, 10.f);
	ConstraintOne->ConstraintInstance.SetLinearXLimit(ELinearConstraintMotion::LCM_Free, 1.f);
	ConstraintOne->ConstraintInstance.SetLinearYLimit(ELinearConstraintMotion::LCM_Free, 1.f);
	ConstraintOne->ConstraintInstance.SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 1.f);

	ConstraintOne->ConstraintInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 1.f);
	ConstraintOne->ConstraintInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 45.f);
	ConstraintOne->ConstraintInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 45.f);

	// Connect the CuttableObject and Blade/Welded Hand as bones with the Constraint
	ConstraintOne->SetConstrainedComponents((UPrimitiveComponent*) GetAttachParent(), FName("Blade"), CuttableComponent, FName("Object"));
}