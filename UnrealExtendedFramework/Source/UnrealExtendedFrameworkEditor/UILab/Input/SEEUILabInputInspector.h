// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Widgets/SCompoundWidget.h"

class FEEUILabInputLog;
struct FEEUILabInputEvent;
template <typename ItemType> class SListView;
class ITableRow;
class STableViewBase;

/**
 * UL-4 input inspector: current focus path and the live event route log
 * (event type, key, handled state) with a clear action.
 */
class SEEUILabInputInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEEUILabInputInspector) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FEEUILabInputLog>& InLog);
	virtual ~SEEUILabInputInspector() override;

private:
	TSharedRef<ITableRow> MakeEventRow(TSharedPtr<FEEUILabInputEvent> Event, const TSharedRef<STableViewBase>& OwnerTable);
	FText GetFocusPathText() const;
	void HandleLogChanged();

	TSharedPtr<FEEUILabInputLog> Log;
	TSharedPtr<SListView<TSharedPtr<FEEUILabInputEvent>>> EventList;
	FDelegateHandle LogChangedHandle;
};

#endif // WITH_EDITOR
