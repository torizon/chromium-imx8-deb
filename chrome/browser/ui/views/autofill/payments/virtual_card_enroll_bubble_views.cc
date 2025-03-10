// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"

#include <memory>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace autofill {

VirtualCardEnrollBubbleViews::VirtualCardEnrollBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetAcceptButtonText());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller->GetDeclineButtonText());
  SetCancelCallback(base::BindOnce(
      &VirtualCardEnrollBubbleViews::OnDialogDeclined, base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(
      &VirtualCardEnrollBubbleViews::OnDialogAccepted, base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  raw_ptr<views::View> legal_message_view =
      SetFootnoteView(CreateLegalMessageView());
  legal_message_view->SetID(DialogViewId::FOOTNOTE_VIEW);
}

VirtualCardEnrollBubbleViews::~VirtualCardEnrollBubbleViews() = default;

void VirtualCardEnrollBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void VirtualCardEnrollBubbleViews::Hide() {
  CloseBubble();
  if (controller_)
    controller_->OnBubbleClosed(closed_reason_);
  controller_ = nullptr;
}

void VirtualCardEnrollBubbleViews::OnDialogAccepted() {
  if (controller_)
    controller_->OnAcceptButton();
}

void VirtualCardEnrollBubbleViews::OnDialogDeclined() {
  if (controller_)
    controller_->OnDeclineButton();
}

void VirtualCardEnrollBubbleViews::AddedToWidget() {
  auto header_view = std::make_unique<views::BoxLayoutView>();
  header_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  header_view->SetInsideBorderInsets(ChromeLayoutProvider::Get()
                                         ->GetInsetsMetric(views::INSETS_DIALOG)
                                         .set_bottom(0));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_DIALOG),
      *bundle.GetImageSkiaNamed(IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_DIALOG_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));

  header_view->AddChildView(std::move(image_view));

  GetBubbleFrameView()->SetHeaderView(std::move(header_view));
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

std::u16string VirtualCardEnrollBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void VirtualCardEnrollBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(closed_reason_);
    controller_ = nullptr;
  }
}

void VirtualCardEnrollBubbleViews::OnWidgetClosing(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  closed_reason_ = GetPaymentsBubbleClosedReasonFromWidgetClosedReason(
      widget->closed_reason());
}

void VirtualCardEnrollBubbleViews::Init() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // If applicable, add the explanation label.  Appears above the card
  // info.
  std::u16string explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* const explanation_label =
        AddChildView(std::make_unique<views::StyledLabel>());
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    explanation_label->SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL);
    explanation_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
    explanation_label->SetText(explanation);

    views::StyledLabel::RangeStyleInfo style_info =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &VirtualCardEnrollBubbleViews::LearnMoreLinkClicked,
            weak_ptr_factory_.GetWeakPtr()));

    uint32_t offset =
        explanation.length() - controller_->GetLearnMoreLinkText().length();
    explanation_label->AddStyleRange(
        gfx::Range(offset,
                   offset + controller_->GetLearnMoreLinkText().length()),
        style_info);
  }

  // Add the card network icon, 'Virtual card', and obfuscated last four digits.
  auto* description_view =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  description_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  description_view->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  const VirtualCardEnrollmentFields virtual_card_enrollment_fields =
      controller_->GetVirtualCardEnrollmentFields();
  CreditCard card = virtual_card_enrollment_fields.credit_card;
  gfx::Image* card_image = virtual_card_enrollment_fields.card_art_image;

  card_network_icon_ =
      description_view->AddChildView(std::make_unique<views::ImageView>());
  DCHECK(!card.network().empty());

  // If the card art image is retrieved at this point, display that. Otherwise
  // fallback to the network icon.
  card_network_icon_->SetImage(
      card_image ? card_image->AsImageSkia()
                 : *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                       CreditCard::IconResourceId(card.network())));
  card_network_icon_->SetTooltipText(card.NetworkForDisplay());

  const std::u16string card_info =
      card.CardIdentifierStringForAutofillDisplay();

  const std::u16string card_label_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX) +
      u"\n" +
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX_TWO) +
      u" " + card_info;

  auto* const card_identifier_label =
      description_view->AddChildView(std::make_unique<views::StyledLabel>());
  card_identifier_label->SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL);
  card_identifier_label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  card_identifier_label->SetText(card_label_text);

  uint32_t length =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX_TWO)
          .length() +
      card_info.length() +
      1;  // one added for space between string and card info.

  uint32_t offset = card_label_text.length() - length;

  views::StyledLabel::RangeStyleInfo linked_styling;
  linked_styling.text_style = views::style::STYLE_SECONDARY;
  card_identifier_label->AddStyleRange(gfx::Range(offset, offset + length),
                                       linked_styling);
}

std::unique_ptr<views::View>
VirtualCardEnrollBubbleViews::CreateLegalMessageView() {
  auto legal_message_view = std::make_unique<views::BoxLayoutView>();
  legal_message_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  legal_message_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));

  const LegalMessageLines google_legal_message =
      controller_->GetVirtualCardEnrollmentFields().google_legal_message;
  const LegalMessageLines issuser_legal_message =
      controller_->GetVirtualCardEnrollmentFields().issuer_legal_message;

  DCHECK(!google_legal_message.empty());
  legal_message_view->AddChildView(std::make_unique<LegalMessageView>(
      google_legal_message,
      base::BindRepeating(
          &VirtualCardEnrollBubbleViews::GoogleLegalMessageClicked,
          base::Unretained(this))));

  if (!issuser_legal_message.empty()) {
    legal_message_view->AddChildView(std::make_unique<LegalMessageView>(
        issuser_legal_message,
        base::BindRepeating(
            &VirtualCardEnrollBubbleViews::IssuerLegalMessageClicked,
            base::Unretained(this))));
  }
  return legal_message_view;
}

void VirtualCardEnrollBubbleViews::LearnMoreLinkClicked() {
  if (controller()) {
    controller()->OnLinkClicked(
        VirtualCardEnrollmentLinkType::VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
        autofill::payments::GetVirtualCardEnrollmentSupportUrl());
  }
}

void VirtualCardEnrollBubbleViews::IssuerLegalMessageClicked(const GURL& url) {
  if (controller()) {
    controller()->OnLinkClicked(
        VirtualCardEnrollmentLinkType::VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
        url);
  }
}

void VirtualCardEnrollBubbleViews::GoogleLegalMessageClicked(const GURL& url) {
  if (controller()) {
    controller()->OnLinkClicked(
        VirtualCardEnrollmentLinkType::
            VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK,
        url);
  }
}

}  // namespace autofill
