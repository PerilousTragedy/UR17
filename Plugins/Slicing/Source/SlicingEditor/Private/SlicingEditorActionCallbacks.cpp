// Copyright 2017, Institute for Artificial Intelligence

#include "SlicingEditorActionCallbacks.h"
#include "SlicingComponent.h"

#include "Core.h"
#include "Editor.h"

#include "Components/BoxComponent.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"

#define LOCTEXT_NAMESPACE "FSlicingEditorModule"

void FSlicingEditorActionCallbacks::ShowInstructions()
{
	const FText InstructionsTitle =
		LOCTEXT("SlicingInstructions_Title", "Instructions to edit an object to be able to slice");
	const FText InstructionsText = FText::FromString(
		FString("To alter an object to be able to slice other objects, ") +
		FString("the user needs to create multiple Sockets on the position - and with the size of - ") +
		FString("certain areas needed for cutting objects. These sockets need specific names to specify ") +
		FString("the area they are used for.\n\nCutting Blade: ") + USlicingComponent::SocketBladeName.ToString() +
		FString("\nCutting Handle: ") + USlicingComponent::SocketHandleName.ToString() +
		FString("\nCutting Exitpoint: ") + USlicingComponent::SocketCuttingExitpointName.ToString());

	// Display the popup-window
	FMessageDialog* Instructions = new FMessageDialog();
	Instructions->Debugf(InstructionsText, &InstructionsTitle);
}

void FSlicingEditorActionCallbacks::OnEnableDebugConsoleOutput(bool* bButtonValue)
{
	*bButtonValue = !*bButtonValue;
}

bool FSlicingEditorActionCallbacks::OnIsEnableDebugConsoleOutputEnabled(bool* bButtonValue)
{
	return *bButtonValue;
}

void FSlicingEditorActionCallbacks::OnEnableDebugShowPlane(bool* bButtonValue)
{
	*bButtonValue = !*bButtonValue;
}

bool FSlicingEditorActionCallbacks::OnIsEnableDebugShowPlaneEnabled(bool* bButtonValue)
{
	return *bButtonValue;
}

void FSlicingEditorActionCallbacks::OnEnableDebugShowTrajectory(bool* bButtonValue)
{
	*bButtonValue = !*bButtonValue;
}

bool FSlicingEditorActionCallbacks::OnIsEnableDebugShowTrajectoryEnabled(bool* bButtonValue)
{
	return *bButtonValue;
}

void FSlicingEditorActionCallbacks::FillSocketsWithComponents()
{
	// Get all selected StaticMeshActors
	TArray<AStaticMeshActor*> SelectedStaticMeshActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AStaticMeshActor>(SelectedStaticMeshActors);

	// DEBUG (console)
	if (SelectedStaticMeshActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Slicing-Plugin: No StaticMeshActor was selected."));
	}

	// Create the BoxComponents for every selected StaticMeshActor
	for (AStaticMeshActor* StaticMeshActor : SelectedStaticMeshActors)
	{
		UStaticMeshComponent* StaticMesh = StaticMeshActor->GetStaticMeshComponent();

		if (StaticMesh->ComponentHasTag(FName("Knife")))
		{
			UE_LOG(LogTemp, Warning, TEXT("Slicing-Plugin: Socket-filling --SUCCESSFUL--"));

			FSlicingEditorActionCallbacks::AddHandleComponent(StaticMesh);
			FSlicingEditorActionCallbacks::AddBladeComponent(StaticMesh);
			FSlicingEditorActionCallbacks::AddCuttingExitpointComponent(StaticMesh);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Slicing-Plugin: Does not have proper sockets."));
		}
	}
}

void FSlicingEditorActionCallbacks::AddBoxComponent(UStaticMeshComponent* StaticMesh, UBoxComponent* BoxComponent, FName SocketName, FName CollisionProfileName, bool bGenerateOverlapEvents)
{
	float SocketToBoxScale = 3.5;

	BoxComponent->RegisterComponent();
	BoxComponent->AttachToComponent(StaticMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
	BoxComponent->SetWorldLocation(StaticMesh->GetSocketLocation(SocketName));
	FVector TempScale = StaticMesh->GetSocketTransform(SocketName).GetScale3D();
	BoxComponent->SetBoxExtent(FVector(1, 1, 1) * SocketToBoxScale);
	BoxComponent->SetCollisionProfileName(CollisionProfileName);
	BoxComponent->bGenerateOverlapEvents = bGenerateOverlapEvents;
	BoxComponent->bMultiBodyOverlap = true;
}

void FSlicingEditorActionCallbacks::AddHandleComponent(UStaticMeshComponent* StaticMesh)
{
	UBoxComponent* HandleComponent = NewObject<UBoxComponent>(StaticMesh, USlicingComponent::SocketHandleName);
	
	FSlicingEditorActionCallbacks::AddBoxComponent(StaticMesh, HandleComponent, USlicingComponent::SocketHandleName, FName("BlockAll"), false);
}

void FSlicingEditorActionCallbacks::AddBladeComponent(UStaticMeshComponent* StaticMesh)
{
	USlicingComponent* BladeComponent = NewObject<USlicingComponent>(StaticMesh, USlicingComponent::SocketBladeName);

	FSlicingEditorActionCallbacks::AddBoxComponent(StaticMesh, BladeComponent, USlicingComponent::SocketBladeName, FName("OverlapAll"), true);
}

void FSlicingEditorActionCallbacks::AddCuttingExitpointComponent(UStaticMeshComponent* StaticMesh)
{
	UBoxComponent* CuttingExitpointComponent = NewObject<UBoxComponent>(StaticMesh, USlicingComponent::SocketCuttingExitpointName);

	FSlicingEditorActionCallbacks::AddBoxComponent(StaticMesh, CuttingExitpointComponent, USlicingComponent::SocketCuttingExitpointName, FName("OverlapAll"), false);
}


void FSlicingEditorActionCallbacks::ReplaceSocketsOfAllStaticMeshComponents()
{
	// To be implemented in the future
}

#undef LOCTEXT_NAMESPACE