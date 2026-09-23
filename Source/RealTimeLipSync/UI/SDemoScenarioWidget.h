// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SWidgetSwitcher;

DECLARE_DELEGATE(FOnDemoStartScenario);
DECLARE_DELEGATE_OneParam(FOnDemoAskQuestion, const FString& /*Question*/);

// Pure Slate UI of the packaged demo, no Blueprint asset. Two screens in a switcher:
//   0. Start screen: opaque background, scenario title/description and a start button.
//   1. Question bar at the bottom of the viewport: free text box + send button (Enter also sends).
// Display only: the owner (ADemoScenarioActor) binds OnStartScenario/OnAskQuestion to its own logic.
class SDemoScenarioWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDemoScenarioWidget) {}
		SLATE_ARGUMENT(FText, ScenarioTitle)
		SLATE_ARGUMENT(FText, ScenarioDescription)
		SLATE_EVENT(FOnDemoStartScenario, OnStartScenario)
		SLATE_EVENT(FOnDemoAskQuestion, OnAskQuestion)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply HandleStartClicked();
	FReply HandleAskClicked();
	void HandleQuestionCommitted(const FText& Text, ETextCommit::Type CommitType);

	// Sends the current text (if not empty) through OnAskQuestion and clears the box.
	void SubmitQuestion();

	FOnDemoStartScenario OnStartScenario;
	FOnDemoAskQuestion OnAskQuestion;

	TSharedPtr<SWidgetSwitcher> ScreenSwitcher;
	TSharedPtr<SEditableTextBox> QuestionBox;
};
