// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraphNode_Base.h"

#include "Logging/TokenizedMessage.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UObject interface
void UEGQuestGraphNode_Base::PostLoad()
{
	Super::PostLoad();
	RegisterListeners();
}

void UEGQuestGraphNode_Base::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		CreateNewGuid();
	}
}

void UEGQuestGraphNode_Base::PostEditImport()
{
	Super::PostEditImport();
	RegisterListeners();
}
// End UObject interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UEdGraphNode interface
void UEGQuestGraphNode_Base::AllocateDefaultPins()
{
	check(Pins.Num() == 0);
	CreateInputPin();
	CreateOutputPin();
}

void UEGQuestGraphNode_Base::ReconstructNode()
{
	// Most likely we also need to make sure the new connections are ok
	Modify();

	// Clear previously set messages
	ErrorMsg.Reset();

	// Drop corrupt links before rebuilding: a link to a pin its owner no longer knows about, or a
	// link the other side does not reciprocate. The latter is what pasted nodes arrive with - their
	// serialized pins still point at pins of nodes that were not copied, one-directionally.
	for (UEdGraphPin* Pin : Pins)
	{
		for (int32 LinkIndex = Pin->LinkedTo.Num() - 1; LinkIndex >= 0; --LinkIndex)
		{
			UEdGraphPin* OtherPin = Pin->LinkedTo[LinkIndex];
			const bool bOtherPinKnown = OtherPin && OtherPin->GetOwningNodeUnchecked() &&
				OtherPin->GetOwningNode()->Pins.Contains(OtherPin);
			if (!bOtherPinKnown || !OtherPin->LinkedTo.Contains(Pin))
			{
				Pin->LinkedTo.RemoveAt(LinkIndex);
			}
		}
	}

	// Move the existing pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Empty();

	// Recreate the new pins
	AllocateDefaultPins();

	// Restore links: pin identity is name + direction, so a stage card's objective pins survive as
	// long as the objective (its GUID names the pin) still exists.
	for (UEdGraphPin* NewPin : Pins)
	{
		UEdGraphPin* const* MatchingOldPin = OldPins.FindByPredicate([NewPin](const UEdGraphPin* OldPin)
		{
			return OldPin && OldPin->Direction == NewPin->Direction && OldPin->PinName == NewPin->PinName;
		});
		if (MatchingOldPin)
		{
			NewPin->CopyPersistentDataFromOldPin(**MatchingOldPin);
		}
	}

	// Throw away the original (old) pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		OldPin->Modify();
		OldPin->BreakAllPinLinks();
		DestroyPin(OldPin);
	}
	OldPins.Empty();
}

// End UEdGraphNode interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions
UEGQuestGraphNode_Base::UEGQuestGraphNode_Base(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bCanRenameNode = false;
}

bool UEGQuestGraphNode_Base::HasOutputConnectionToNode(const UEdGraphNode* TargetNode) const
{
	for (UEdGraphPin* ChildInputPin : GetOutputPin()->LinkedTo)
	{
		if (ChildInputPin->GetOwningNode() == TargetNode)
		{
			return true;
		}
	}

	return false;;
}

void UEGQuestGraphNode_Base::ClearCompilerMessage()
{
	bHasCompilerMessage = false;
	ErrorType = EMessageSeverity::Info;
	ErrorMsg.Empty();
}

// Severity is hardcoded because the signature carries none: a diagnostic's severity cannot reach here.
// Callers that have one (UEGQuestGraphNode::ApplyCompilerWarnings, and the compiler's
// ApplyDiagnosticsToGraphNodes) overwrite ErrorType afterwards. That correction belongs in this
// function, as a Severity parameter, the moment the declaration can take one.
void UEGQuestGraphNode_Base::SetCompilerWarningMessage(FString Message)
{
	bHasCompilerMessage = true;
	ErrorType = EMessageSeverity::Warning;
	ErrorMsg = MoveTemp(Message);
}

void UEGQuestGraphNode_Base::RegisterListeners()
{
	GetQuest()->OnQuestPropertyChanged.AddUObject(this, &UEGQuestGraphNode_Base::OnQuestPropertyChanged);
}
// End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
