// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDemoScenarioWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const int32 StartScreenIndex = 0;
	const int32 QuestionBarIndex = 1;

	// Start screen text column, in Slate units.
	const float StartScreenMaxWidth = 700.f;
	const float StartScreenWrapWidth = 640.f;
}

void SDemoScenarioWidget::Construct(const FArguments& InArgs)
{
	OnStartScenario = InArgs._OnStartScenario;
	OnAskQuestion = InArgs._OnAskQuestion;

	ChildSlot
	[
		SAssignNew(ScreenSwitcher, SWidgetSwitcher)
		.WidgetIndex(StartScreenIndex)

		// 0. Start screen, covers the whole scene.
		+ SWidgetSwitcher::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 1.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MaxDesiredWidth(StartScreenMaxWidth)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 16.f)
					[
						SNew(STextBlock)
						.Text(InArgs._ScenarioTitle)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 32))
						.Justification(ETextJustify::Center)
						.WrapTextAt(StartScreenWrapWidth)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 32.f)
					[
						SNew(STextBlock)
						.Text(InArgs._ScenarioDescription)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
						.Justification(ETextJustify::Center)
						// Fixed width rather than AutoWrapText: auto wrap uses the previous frame's
						// geometry and overflows slightly (lines clipped on both sides) when centered.
						.WrapTextAt(StartScreenWrapWidth)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.ContentPadding(FMargin(24.f, 10.f))
						.OnClicked(this, &SDemoScenarioWidget::HandleStartClicked)
						[
							SNew(STextBlock)
							.Text(INVTEXT("Szenario starten"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
						]
					]
				]
			]
		]

		// 1. Question bar at the bottom; the rest of the screen lets the scene show through.
		+ SWidgetSwitcher::Slot()
		.VAlign(VAlign_Bottom)
		.Padding(40.f, 0.f, 40.f, 40.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SAssignNew(QuestionBox, SEditableTextBox)
				.HintText(INVTEXT("Ihre Frage ..."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
				.OnTextCommitted(this, &SDemoScenarioWidget::HandleQuestionCommitted)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(FMargin(16.f, 6.f))
				.OnClicked(this, &SDemoScenarioWidget::HandleAskClicked)
				[
					SNew(STextBlock)
					.Text(INVTEXT("Fragen"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
				]
			]
		]
	];
}

FReply SDemoScenarioWidget::HandleStartClicked()
{
	ScreenSwitcher->SetActiveWidgetIndex(QuestionBarIndex);
	FSlateApplication::Get().SetKeyboardFocus(QuestionBox);
	OnStartScenario.ExecuteIfBound();
	return FReply::Handled();
}

FReply SDemoScenarioWidget::HandleAskClicked()
{
	SubmitQuestion();
	return FReply::Handled();
}

void SDemoScenarioWidget::HandleQuestionCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		SubmitQuestion();
	}
}

void SDemoScenarioWidget::SubmitQuestion()
{
	const FString Question = QuestionBox->GetText().ToString().TrimStartAndEnd();
	if (Question.IsEmpty())
	{
		return;
	}

	QuestionBox->SetText(FText::GetEmpty());
	OnAskQuestion.ExecuteIfBound(Question);
}
