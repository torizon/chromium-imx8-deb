// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

#include <memory>

#include "base/check_op.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigate_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_current_entry_change_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_navigate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_reload_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_transition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_update_current_entry_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api_navigation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_current_entry_change_event.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_destination.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_transition.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

class NavigateReaction final : public ScriptFunction::Callable {
 public:
  enum class ResolveType { kFulfill, kReject };
  enum class ReactType { kImmediate, kTransitionWhile };
  static void React(ScriptState* script_state,
                    ScriptPromise promise,
                    NavigationApiNavigation* navigation,
                    AbortSignal* signal,
                    bool should_reset_focus,
                    ReactType react_type) {
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     script_state, MakeGarbageCollected<NavigateReaction>(
                                       navigation, signal, should_reset_focus,
                                       ResolveType::kFulfill, react_type)),
                 MakeGarbageCollected<ScriptFunction>(
                     script_state, MakeGarbageCollected<NavigateReaction>(
                                       navigation, signal, should_reset_focus,
                                       ResolveType::kReject, react_type)));
  }

  NavigateReaction(NavigationApiNavigation* navigation,
                   AbortSignal* signal,
                   bool should_reset_focus,
                   ResolveType resolve_type,
                   ReactType react_type)
      : navigation_(navigation),
        signal_(signal),
        should_reset_focus_(should_reset_focus),
        resolve_type_(resolve_type),
        react_type_(react_type) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Callable::Trace(visitor);
    visitor->Trace(navigation_);
    visitor->Trace(signal_);
  }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    auto* window = LocalDOMWindow::From(script_state);
    DCHECK(window);
    if (signal_->aborted()) {
      return ScriptValue();
    }

    NavigationApi* navigation_api = NavigationApi::navigation(*window);
    navigation_api->ongoing_navigation_signal_ = nullptr;

    if (resolve_type_ == ResolveType::kFulfill) {
      navigation_api->ResolvePromisesAndFireNavigateSuccessEvent(navigation_);
    } else {
      navigation_api->RejectPromisesAndFireNavigateErrorEvent(navigation_,
                                                              value);
    }

    if (should_reset_focus_) {
      auto* document = navigation_api->GetSupplementable()->document();
      if (Element* focus_delegate = document->GetAutofocusDelegate()) {
        focus_delegate->focus();
      } else {
        document->ClearFocusedElement();
      }
    }

    if (react_type_ == ReactType::kTransitionWhile && window->GetFrame()) {
      window->GetFrame()->Loader().DidFinishNavigation(
          resolve_type_ == ResolveType::kFulfill
              ? FrameLoader::NavigationFinishState::kSuccess
              : FrameLoader::NavigationFinishState::kFailure);
    }

    return ScriptValue();
  }

 private:
  Member<NavigationApiNavigation> navigation_;
  Member<AbortSignal> signal_;
  bool should_reset_focus_;
  ResolveType resolve_type_;
  ReactType react_type_;
};

template <typename... DOMExceptionArgs>
NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   DOMExceptionArgs&&... args) {
  auto* ex = MakeGarbageCollected<DOMException>(
      std::forward<DOMExceptionArgs>(args)...);
  return EarlyErrorResult(script_state, ex);
}

NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   DOMException* ex) {
  auto* result = NavigationResult::Create();
  result->setCommitted(ScriptPromise::RejectWithDOMException(script_state, ex));
  result->setFinished(ScriptPromise::RejectWithDOMException(script_state, ex));
  return result;
}

NavigationResult* EarlyErrorResult(ScriptState* script_state,
                                   v8::Local<v8::Value> ex) {
  auto* result = NavigationResult::Create();
  result->setCommitted(ScriptPromise::Reject(script_state, ex));
  result->setFinished(ScriptPromise::Reject(script_state, ex));
  return result;
}

NavigationResult* EarlySuccessResult(ScriptState* script_state,
                                     NavigationHistoryEntry* entry) {
  auto* result = NavigationResult::Create();
  result->setCommitted(
      ScriptPromise::Cast(script_state, ToV8(entry, script_state)));
  result->setFinished(
      ScriptPromise::Cast(script_state, ToV8(entry, script_state)));
  return result;
}

String DetermineNavigationType(WebFrameLoadType type) {
  switch (type) {
    case WebFrameLoadType::kStandard:
      return "push";
    case WebFrameLoadType::kBackForward:
      return "traverse";
    case WebFrameLoadType::kReload:
    case WebFrameLoadType::kReloadBypassingCache:
      return "reload";
    case WebFrameLoadType::kReplaceCurrentItem:
      return "replace";
  }
  NOTREACHED();
  return String();
}

const char NavigationApi::kSupplementName[] = "NavigationApi";

NavigationApi* NavigationApi::navigation(LocalDOMWindow& window) {
  return RuntimeEnabledFeatures::AppHistoryEnabled(&window) ? From(window)
                                                            : nullptr;
}

NavigationApi* NavigationApi::From(LocalDOMWindow& window) {
  auto* navigation_api =
      Supplement<LocalDOMWindow>::From<NavigationApi>(window);
  if (!navigation_api) {
    navigation_api = MakeGarbageCollected<NavigationApi>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, navigation_api);
  }
  return navigation_api;
}

NavigationApi::NavigationApi(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void NavigationApi::setOnnavigate(EventListener* listener) {
  UseCounter::Count(GetSupplementable(), WebFeature::kAppHistory);
  SetAttributeEventListener(event_type_names::kNavigate, listener);
}

void NavigationApi::PopulateKeySet() {
  DCHECK(keys_to_indices_.IsEmpty());
  for (wtf_size_t i = 0; i < entries_.size(); i++)
    keys_to_indices_.insert(entries_[i]->key(), i);
}

void NavigationApi::InitializeForNewWindow(
    HistoryItem& current,
    WebFrameLoadType load_type,
    CommitReason commit_reason,
    NavigationApi* previous,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries) {
  DCHECK(entries_.IsEmpty());

  // This can happen even when commit_reason is not kInitialization, e.g. when
  // navigating from about:blank#1 to about:blank#2 where both are initial
  // about:blanks.
  if (HasEntriesAndEventsDisabled())
    return;

  // Under most circumstances, the browser process provides the information
  // need to initialize the navigation API's entries array from |back_entries|
  // and |forward_entries|. However, these are not available when the renderer
  // handles the navigation entirely, so in those cases (javascript: urls, XSLT
  // commits, and non-back/forward about:blank), copy the array from the
  // previous window and use the same update algorithm as same-document
  // navigations.
  if (commit_reason != CommitReason::kRegular ||
      (current.Url() == BlankURL() && !IsBackForwardLoadType(load_type)) ||
      (current.Url().IsAboutSrcdocURL() && !IsBackForwardLoadType(load_type))) {
    if (previous && !previous->entries_.IsEmpty()) {
      CloneFromPrevious(*previous);
      UpdateForNavigation(current, load_type);
      return;
    }
  }

  // Construct |entries_|. Any back entries are inserted, then the current
  // entry, then any forward entries.
  entries_.ReserveCapacity(base::checked_cast<wtf_size_t>(
      back_entries.size() + forward_entries.size() + 1));
  for (const auto& entry : back_entries) {
    entries_.emplace_back(MakeGarbageCollected<NavigationHistoryEntry>(
        GetSupplementable(), entry));
  }

  current_entry_index_ = base::checked_cast<wtf_size_t>(back_entries.size());
  entries_.emplace_back(MakeGarbageCollected<NavigationHistoryEntry>(
      GetSupplementable(), &current));

  for (const auto& entry : forward_entries) {
    entries_.emplace_back(MakeGarbageCollected<NavigationHistoryEntry>(
        GetSupplementable(), entry));
  }
  PopulateKeySet();
}

void NavigationApi::CloneFromPrevious(NavigationApi& previous) {
  DCHECK(entries_.IsEmpty());
  entries_.ReserveCapacity(previous.entries_.size());
  for (wtf_size_t i = 0; i < previous.entries_.size(); i++) {
    // It's possible that |old_item| is indirectly holding a reference to
    // the old Document. Also, it has a bunch of state we don't need for a
    // non-current entry. Clone a subset of its state to a |new_item|.
    // NOTE: values copied here should also be copied in GetEntryForRestore().
    HistoryItem* old_item = previous.entries_[i]->GetItem();
    HistoryItem* new_item = MakeGarbageCollected<HistoryItem>();
    new_item->SetItemSequenceNumber(old_item->ItemSequenceNumber());
    new_item->SetDocumentSequenceNumber(old_item->DocumentSequenceNumber());
    new_item->SetURL(old_item->Url());
    new_item->SetNavigationApiKey(old_item->GetNavigationApiKey());
    new_item->SetNavigationApiId(old_item->GetNavigationApiId());
    new_item->SetNavigationApiState(old_item->GetNavigationApiState());
    entries_.emplace_back(MakeGarbageCollected<NavigationHistoryEntry>(
        GetSupplementable(), new_item));
  }
  current_entry_index_ = previous.current_entry_index_;
  PopulateKeySet();
}

void NavigationApi::UpdateForNavigation(HistoryItem& item,
                                        WebFrameLoadType type) {
  // A same-document navigation (e.g., a document.open()) in a
  // |HasEntriesAndEventsDisabled()| situation will try to operate on an empty
  // |entries_|. The navigation API considers this a no-op.
  if (entries_.IsEmpty())
    return;

  NavigationHistoryEntry* old_current = currentEntry();

  HeapVector<Member<NavigationHistoryEntry>> disposed_entries;
  if (type == WebFrameLoadType::kBackForward) {
    // If this is a same-document back/forward navigation, the new current
    // entry should already be present in entries_ and its key in
    // keys_to_indices_.
    DCHECK(keys_to_indices_.Contains(item.GetNavigationApiKey()));
    current_entry_index_ = keys_to_indices_.at(item.GetNavigationApiKey());
  } else if (type == WebFrameLoadType::kStandard) {
    // For a new back/forward entry, truncate any forward entries and prepare
    // to append.
    current_entry_index_++;
    for (wtf_size_t i = current_entry_index_; i < entries_.size(); i++) {
      keys_to_indices_.erase(entries_[i]->key());
      disposed_entries.push_back(entries_[i]);
    }
    entries_.resize(current_entry_index_ + 1);
  } else if (type == WebFrameLoadType::kReplaceCurrentItem) {
    DCHECK_NE(current_entry_index_, -1);
    disposed_entries.push_back(entries_[current_entry_index_]);
  }

  if (type == WebFrameLoadType::kStandard ||
      type == WebFrameLoadType::kReplaceCurrentItem) {
    // current_index_ is now correctly set (for type of
    // WebFrameLoadType::kReplaceCurrentItem, it didn't change). Create the new
    // current entry.
    entries_[current_entry_index_] =
        MakeGarbageCollected<NavigationHistoryEntry>(GetSupplementable(),
                                                     &item);
    keys_to_indices_.insert(entries_[current_entry_index_]->key(),
                            current_entry_index_);
  }

  // Note how reload types don't update the current entry or dispose any
  // entries.

  // It's important to do this before firing dispose events, since dispose
  // events could start another navigation or otherwise mess with
  // ongoing_navigation_.
  if (ongoing_navigation_) {
    ongoing_navigation_->NotifyAboutTheCommittedToEntry(
        entries_[current_entry_index_]);
  }

  auto* init = NavigationCurrentEntryChangeEventInit::Create();
  init->setNavigationType(DetermineNavigationType(type));
  init->setFrom(old_current);
  DispatchEvent(*NavigationCurrentEntryChangeEvent::Create(
      event_type_names::kCurrententrychange, init));

  for (const auto& disposed_entry : disposed_entries) {
    disposed_entry->DispatchEvent(*Event::Create(event_type_names::kDispose));
  }
}

NavigationHistoryEntry* NavigationApi::GetEntryForRestore(
    const mojom::blink::NavigationApiHistoryEntryPtr& entry) {
  const auto& it = keys_to_indices_.find(entry->key);
  if (it != keys_to_indices_.end()) {
    NavigationHistoryEntry* existing_entry = entries_[it->value];
    if (existing_entry->id() == entry->id)
      return existing_entry;
  }
  // NOTE: values copied here should also be copied in CloneFromPrevious().
  // TODO(japhet): Figure out if there's a way to better share logic with
  // CloneFromPrevious().
  HistoryItem* item = MakeGarbageCollected<HistoryItem>();
  item->SetItemSequenceNumber(entry->item_sequence_number);
  item->SetDocumentSequenceNumber(entry->document_sequence_number);
  item->SetURLString(entry->url);
  item->SetNavigationApiKey(entry->key);
  item->SetNavigationApiId(entry->id);
  item->SetNavigationApiState(SerializedScriptValue::Create(entry->state));
  return MakeGarbageCollected<NavigationHistoryEntry>(GetSupplementable(),
                                                      item);
}

// static
void FireDisposeEventsAsync(
    HeapVector<Member<NavigationHistoryEntry>>* disposed_entries) {
  for (const auto& entry : *disposed_entries) {
    entry->DispatchEvent(*Event::Create(event_type_names::kDispose));
  }
}

void NavigationApi::SetEntriesForRestore(
    const mojom::blink::NavigationApiHistoryEntryArraysPtr& entry_arrays) {
  // If this window HasEntriesAndEventsDisabled(), we shouldn't attempt to
  // restore anything.
  if (HasEntriesAndEventsDisabled())
    return;

  HeapVector<Member<NavigationHistoryEntry>> new_entries;
  new_entries.ReserveCapacity(
      base::checked_cast<wtf_size_t>(entry_arrays->back_entries.size() +
                                     entry_arrays->forward_entries.size() + 1));
  for (const auto& item : entry_arrays->back_entries)
    new_entries.emplace_back(GetEntryForRestore(item));
  new_entries.emplace_back(currentEntry());
  for (const auto& item : entry_arrays->forward_entries)
    new_entries.emplace_back(GetEntryForRestore(item));

  new_entries.swap(entries_);
  current_entry_index_ =
      base::checked_cast<wtf_size_t>(entry_arrays->back_entries.size());
  keys_to_indices_.clear();
  PopulateKeySet();

  // |new_entries| now contains the previous entries_. Find the ones that are no
  // longer in entries_ so they can be disposed.
  HeapVector<Member<NavigationHistoryEntry>>* disposed_entries =
      MakeGarbageCollected<HeapVector<Member<NavigationHistoryEntry>>>();
  for (const auto& entry : new_entries) {
    const auto& it = keys_to_indices_.find(entry->key());
    if (it == keys_to_indices_.end() || entries_[it->value] != entry)
      disposed_entries->push_back(entry);
  }
  GetSupplementable()
      ->GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, WTF::Bind(&FireDisposeEventsAsync,
                                      WrapPersistent(disposed_entries)));
}

NavigationHistoryEntry* NavigationApi::currentEntry() const {
  // current_index_ is initialized to -1 and set >= 0 when entries_ is
  // populated. It will still be negative if the navigation object of an initial
  // empty document or opaque-origin document is accessed.
  return !HasEntriesAndEventsDisabled() && current_entry_index_ >= 0
             ? entries_[current_entry_index_]
             : nullptr;
}

HeapVector<Member<NavigationHistoryEntry>> NavigationApi::entries() {
  return HasEntriesAndEventsDisabled()
             ? HeapVector<Member<NavigationHistoryEntry>>()
             : entries_;
}

void NavigationApi::updateCurrentEntry(
    NavigationUpdateCurrentEntryOptions* options,
    ExceptionState& exception_state) {
  NavigationHistoryEntry* current_entry = currentEntry();

  if (!current_entry) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "updateCurrent() cannot be called when navigation.current is null.");
    return;
  }

  scoped_refptr<SerializedScriptValue> serialized_state =
      SerializeState(options->state(), exception_state);
  if (exception_state.HadException())
    return;

  current_entry->GetItem()->SetNavigationApiState(std::move(serialized_state));

  auto* init = NavigationCurrentEntryChangeEventInit::Create();
  init->setFrom(current_entry);
  DispatchEvent(*NavigationCurrentEntryChangeEvent::Create(
      event_type_names::kCurrententrychange, init));
}

NavigationResult* NavigationApi::navigate(ScriptState* script_state,
                                          const String& url,
                                          NavigationNavigateOptions* options) {
  KURL completed_url = GetSupplementable()->CompleteURL(url);
  if (!completed_url.IsValid()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kSyntaxError,
                            "Invalid URL '" + completed_url.GetString() + "'.");
  }

  if (DOMException* maybe_ex = PerformSharedNavigationChecks("navigate()"))
    return EarlyErrorResult(script_state, maybe_ex);

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      ExceptionState exception_state(
          script_state->GetIsolate(),
          ExceptionContext::Context::kOperationInvoke, "Navigation",
          "navigate");
      serialized_state = SerializeState(options->state(), exception_state);
      if (exception_state.HadException()) {
        NavigationResult* result =
            EarlyErrorResult(script_state, exception_state.GetException());
        exception_state.ClearException();
        return result;
      }
    }
  }

  WebFrameLoadType frame_load_type = options->replace()
                                         ? WebFrameLoadType::kReplaceCurrentItem
                                         : WebFrameLoadType::kStandard;

  return PerformNonTraverseNavigation(script_state, completed_url,
                                      std::move(serialized_state), options,
                                      frame_load_type);
}

NavigationResult* NavigationApi::reload(ScriptState* script_state,
                                        NavigationReloadOptions* options) {
  if (DOMException* maybe_ex = PerformSharedNavigationChecks("reload()"))
    return EarlyErrorResult(script_state, maybe_ex);

  scoped_refptr<SerializedScriptValue> serialized_state = nullptr;
  {
    if (options->hasState()) {
      ExceptionState exception_state(
          script_state->GetIsolate(),
          ExceptionContext::Context::kOperationInvoke, "Navigation", "reload");
      serialized_state = SerializeState(options->state(), exception_state);
      if (exception_state.HadException()) {
        NavigationResult* result =
            EarlyErrorResult(script_state, exception_state.GetException());
        exception_state.ClearException();
        return result;
      }
    } else if (NavigationHistoryEntry* current_entry = currentEntry()) {
      serialized_state = current_entry->GetItem()->GetNavigationApiState();
    }
  }

  return PerformNonTraverseNavigation(script_state, GetSupplementable()->Url(),
                                      std::move(serialized_state), options,
                                      WebFrameLoadType::kReload);
}

NavigationResult* NavigationApi::PerformNonTraverseNavigation(
    ScriptState* script_state,
    const KURL& url,
    scoped_refptr<SerializedScriptValue> serialized_state,
    NavigationOptions* options,
    WebFrameLoadType frame_load_type) {
  DCHECK(frame_load_type == WebFrameLoadType::kReplaceCurrentItem ||
         frame_load_type == WebFrameLoadType::kReload ||
         frame_load_type == WebFrameLoadType::kStandard);

  NavigationApiNavigation* navigation =
      MakeGarbageCollected<NavigationApiNavigation>(
          script_state, this, options, String(), std::move(serialized_state));
  upcoming_non_traversal_navigation_ = navigation;

  GetSupplementable()->GetFrame()->MaybeLogAdClickNavigation();

  FrameLoadRequest request(GetSupplementable(), ResourceRequest(url));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);
  GetSupplementable()->GetFrame()->Navigate(request, frame_load_type);

  // DispatchNavigateEvent() will clear upcoming_non_traversal_navigation_ if we
  // get that far. If the navigation is blocked before DispatchNavigateEvent()
  // is called, reject the promise and cleanup here.
  if (upcoming_non_traversal_navigation_ == navigation) {
    upcoming_non_traversal_navigation_ = nullptr;
    return EarlyErrorResult(script_state, DOMExceptionCode::kAbortError,
                            "Navigation was aborted");
  }

  if (SerializedScriptValue* state = navigation->TakeSerializedState()) {
    currentEntry()->GetItem()->SetNavigationApiState(state);
  }
  return navigation->GetNavigationResult();
}

NavigationResult* NavigationApi::traverseTo(ScriptState* script_state,
                                            const String& key,
                                            NavigationOptions* options) {
  if (DOMException* maybe_ex =
          PerformSharedNavigationChecks("traverseTo()/back()/forward()")) {
    return EarlyErrorResult(script_state, maybe_ex);
  }

  if (!keys_to_indices_.Contains(key)) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Invalid key");
  }
  if (key == currentEntry()->key()) {
    return EarlySuccessResult(script_state, currentEntry());
  }

  auto previous_navigation = upcoming_traversals_.find(key);
  if (previous_navigation != upcoming_traversals_.end())
    return previous_navigation->value->GetNavigationResult();

  NavigationApiNavigation* ongoing_navigation =
      MakeGarbageCollected<NavigationApiNavigation>(script_state, this, options,
                                                    key);
  upcoming_traversals_.insert(key, ongoing_navigation);
  GetSupplementable()
      ->GetFrame()
      ->GetLocalFrameHostRemote()
      .NavigateToNavigationApiKey(key, LocalFrame::HasTransientUserActivation(
                                           GetSupplementable()->GetFrame()));
  return ongoing_navigation->GetNavigationResult();
}

bool NavigationApi::canGoBack() const {
  return !HasEntriesAndEventsDisabled() && current_entry_index_ > 0;
}

bool NavigationApi::canGoForward() const {
  return !HasEntriesAndEventsDisabled() && current_entry_index_ != -1 &&
         static_cast<size_t>(current_entry_index_) < entries_.size() - 1;
}

NavigationResult* NavigationApi::back(ScriptState* script_state,
                                      NavigationOptions* options) {
  if (!canGoBack()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Cannot go back");
  }
  return traverseTo(script_state, entries_[current_entry_index_ - 1]->key(),
                    options);
}

NavigationResult* NavigationApi::forward(ScriptState* script_state,
                                         NavigationOptions* options) {
  if (!canGoForward()) {
    return EarlyErrorResult(script_state, DOMExceptionCode::kInvalidStateError,
                            "Cannot go forward");
  }
  return traverseTo(script_state, entries_[current_entry_index_ + 1]->key(),
                    options);
}

DOMException* NavigationApi::PerformSharedNavigationChecks(
    const String& method_name_for_error_message) {
  if (!GetSupplementable()->GetFrame()) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called when the Window is detached.");
  }
  if (GetSupplementable()->document()->PageDismissalEventBeingDispatched()) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        method_name_for_error_message +
            " cannot be called during unload or beforeunload.");
  }
  return nullptr;
}

scoped_refptr<SerializedScriptValue> NavigationApi::SerializeState(
    const ScriptValue& value,
    ExceptionState& exception_state) {
  return SerializedScriptValue::Serialize(
      GetSupplementable()->GetIsolate(), value.V8Value(),
      SerializedScriptValue::SerializeOptions(
          SerializedScriptValue::kForStorage),
      exception_state);
}

void NavigationApi::PromoteUpcomingNavigationToOngoing(const String& key) {
  DCHECK(!ongoing_navigation_);
  if (!key.IsNull()) {
    DCHECK(!upcoming_non_traversal_navigation_);
    auto iter = upcoming_traversals_.find(key);
    if (iter != upcoming_traversals_.end()) {
      ongoing_navigation_ = iter->value;
      upcoming_traversals_.erase(iter);
    }
  } else {
    ongoing_navigation_ = upcoming_non_traversal_navigation_.Release();
  }
}

bool NavigationApi::HasEntriesAndEventsDisabled() const {
  auto* frame = GetSupplementable()->GetFrame();
  return !frame ||
         !GetSupplementable()
              ->GetFrame()
              ->Loader()
              .HasLoadedNonInitialEmptyDocument() ||
         GetSupplementable()->GetSecurityOrigin()->IsOpaque();
}

NavigationApi::DispatchResult NavigationApi::DispatchNavigateEvent(
    const KURL& url,
    HTMLFormElement* form,
    NavigateEventType event_type,
    WebFrameLoadType type,
    UserNavigationInvolvement involvement,
    SerializedScriptValue* state_object,
    HistoryItem* destination_item,
    bool is_browser_initiated,
    bool is_synchronously_committed) {
  // TODO(japhet): The draft spec says to cancel any ongoing navigate event
  // before invoking DispatchNavigateEvent(), because not all navigations will
  // fire a navigate event, but all should abort an ongoing navigate event.
  // The main case were that would be a problem (browser-initiated back/forward)
  // is not implemented yet. Move this once it is implemented.
  InformAboutCanceledNavigation();

  const KURL& current_url = GetSupplementable()->Url();
  const String& key =
      destination_item ? destination_item->GetNavigationApiKey() : String();
  PromoteUpcomingNavigationToOngoing(key);

  if (HasEntriesAndEventsDisabled()) {
    if (ongoing_navigation_) {
      // The spec only does the equivalent of CleanupApiNavigation() + resetting
      // the state, but we need to detach promise resolvers for this case since
      // we will never resolve the finished/committed promises.
      ongoing_navigation_->CleanupForCrossDocument();
    }
    return DispatchResult::kContinue;
  }

  auto* script_state =
      ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
  ScriptState::Scope scope(script_state);

  if (type == WebFrameLoadType::kBackForward &&
      event_type == NavigateEventType::kFragment &&
      !keys_to_indices_.Contains(key)) {
    // This same document history traversal was preempted by another navigation
    // that removed this entry from the back/forward list. Proceeding will leave
    // entries_ out of sync with the browser process.
    FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
    return DispatchResult::kAbort;
  }

  auto* init = NavigateEventInit::Create();
  const String& navigation_type = DetermineNavigationType(type);
  init->setNavigationType(navigation_type);

  SerializedScriptValue* destination_state = nullptr;
  if (destination_item)
    destination_state = destination_item->GetNavigationApiState();
  else if (ongoing_navigation_)
    destination_state = ongoing_navigation_->GetSerializedState();
  NavigationDestination* destination =
      MakeGarbageCollected<NavigationDestination>(
          url, event_type != NavigateEventType::kCrossDocument,
          destination_state);
  if (type == WebFrameLoadType::kBackForward) {
    auto iter = keys_to_indices_.find(key);
    int index = iter == keys_to_indices_.end() ? 0 : iter->value;
    destination->SetTraverseProperties(
        key, destination_item->GetNavigationApiId(), index);
  }
  init->setDestination(destination);

  init->setCancelable(type != WebFrameLoadType::kBackForward);
  init->setCanTransition(
      CanChangeToUrlForHistoryApi(url, GetSupplementable()->GetSecurityOrigin(),
                                  current_url) &&
      (event_type != NavigateEventType::kCrossDocument ||
       type != WebFrameLoadType::kBackForward));
  init->setHashChange(event_type == NavigateEventType::kFragment &&
                      url != current_url &&
                      EqualIgnoringFragmentIdentifier(url, current_url));

  init->setUserInitiated(involvement != UserNavigationInvolvement::kNone);
  if (form && form->Method() == FormSubmission::kPostMethod) {
    init->setFormData(FormData::Create(form, ASSERT_NO_EXCEPTION));
  }
  if (ongoing_navigation_)
    init->setInfo(ongoing_navigation_->GetInfo());
  init->setSignal(MakeGarbageCollected<AbortSignal>(GetSupplementable()));
  auto* navigate_event = NavigateEvent::Create(
      GetSupplementable(), event_type_names::kNavigate, init);
  navigate_event->SetUrl(url);

  DCHECK(!ongoing_navigate_event_);
  DCHECK(!ongoing_navigation_signal_);
  ongoing_navigate_event_ = navigate_event;
  ongoing_navigation_signal_ = navigate_event->signal();
  DispatchEvent(*navigate_event);
  ongoing_navigate_event_ = nullptr;

  if (navigate_event->defaultPrevented()) {
    if (!navigate_event->signal()->aborted())
      FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
    return DispatchResult::kAbort;
  }

  auto promise_list = navigate_event->GetNavigationActionPromisesList();
  if (!promise_list.IsEmpty()) {
    transition_ = MakeGarbageCollected<NavigationTransition>(
        script_state, navigation_type, currentEntry());
    // In the spec, the URL and history update steps are not called for reloads.
    // In our implementation, we call the corresponding function anyway, but
    // |type| being a reload type makes it do none of the spec-relevant
    // steps. Instead it does stuff like the loading spinner and use counters.
    GetSupplementable()->document()->Loader()->RunURLAndHistoryUpdateSteps(
        url, destination_item,
        mojom::blink::SameDocumentNavigationType::kNavigationApiTransitionWhile,
        state_object, type, is_browser_initiated, is_synchronously_committed);
  }

  if (!promise_list.IsEmpty() ||
      event_type != NavigateEventType::kCrossDocument) {
    NavigateReaction::ReactType react_type =
        promise_list.IsEmpty() ? NavigateReaction::ReactType::kImmediate
                               : NavigateReaction::ReactType::kTransitionWhile;

    // There is a subtle timing difference between the fast-path for zero
    // promises and the path for 1+ promises, in both spec and implementation.
    // In most uses of ScriptPromise::All / the Web IDL spec's "wait for all",
    // this does not matter. However for us there are so many events and promise
    // handlers firing around the same time (navigatesuccess, committed promise,
    // finished promise, ...) that the difference is pretty easily observable by
    // web developers and web platform tests. So, let's make sure we always go
    // down the 1+ promises path.
    const HeapVector<ScriptPromise>& tweaked_promise_list =
        promise_list.IsEmpty()
            ? HeapVector<ScriptPromise>(
                  {ScriptPromise::CastUndefined(script_state)})
            : promise_list;

    NavigateReaction::React(
        script_state, ScriptPromise::All(script_state, tweaked_promise_list),
        ongoing_navigation_, navigate_event->signal(),
        navigate_event->ShouldResetFocus(), react_type);
  } else if (ongoing_navigation_) {
    // The spec assumes it's ok to leave a promise permanently unresolved, but
    // ScriptPromiseResolver requires either resolution or explicit detach.
    ongoing_navigation_->CleanupForCrossDocument();
  }

  return promise_list.IsEmpty() ? DispatchResult::kContinue
                                : DispatchResult::kTransitionWhile;
}

void NavigationApi::InformAboutCanceledNavigation() {
  if (ongoing_navigation_signal_) {
    auto* script_state =
        ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
    ScriptState::Scope scope(script_state);
    FinalizeWithAbortedNavigationError(script_state, ongoing_navigation_);
  }

  // If this function is being called as part of frame detach, also cleanup any
  // upcoming_traversals_.
  //
  // This function may be called when a v8 context hasn't been initialized.
  // upcoming_traversals_ being non-empty requires a v8 context, so check that
  // so that we don't unnecessarily try to initialize one below.
  if (!upcoming_traversals_.IsEmpty() && GetSupplementable()->GetFrame() &&
      !GetSupplementable()->GetFrame()->IsAttached()) {
    auto* script_state =
        ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
    ScriptState::Scope scope(script_state);

    HeapVector<Member<NavigationApiNavigation>> traversals;
    CopyValuesToVector(upcoming_traversals_, traversals);
    for (auto& traversal : traversals)
      FinalizeWithAbortedNavigationError(script_state, traversal);
    DCHECK(upcoming_traversals_.IsEmpty());
  }
}

void NavigationApi::RejectPromisesAndFireNavigateErrorEvent(
    NavigationApiNavigation* navigation,
    ScriptValue value) {
  auto* isolate = GetSupplementable()->GetIsolate();
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, value.V8Value());
  std::unique_ptr<SourceLocation> location =
      SourceLocation::FromMessage(isolate, message, GetSupplementable());
  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(message->Get()), std::move(location), value,
      &DOMWrapperWorld::MainWorld());
  event->SetType(event_type_names::kNavigateerror);
  DispatchEvent(*event);

  if (navigation)
    navigation->RejectFinishedPromise(value);

  if (transition_) {
    transition_->RejectFinishedPromise(value);
    transition_ = nullptr;
  }
}

void NavigationApi::ResolvePromisesAndFireNavigateSuccessEvent(
    NavigationApiNavigation* navigation) {
  DispatchEvent(*Event::Create(event_type_names::kNavigatesuccess));

  if (navigation)
    navigation->ResolveFinishedPromise();

  if (transition_) {
    transition_->ResolveFinishedPromise();
    transition_ = nullptr;
  }
}

void NavigationApi::CleanupApiNavigation(NavigationApiNavigation& navigation) {
  if (&navigation == ongoing_navigation_) {
    ongoing_navigation_ = nullptr;
  } else {
    DCHECK(!navigation.GetKey().IsNull());
    DCHECK(upcoming_traversals_.Contains(navigation.GetKey()));
    upcoming_traversals_.erase(navigation.GetKey());
  }
}

void NavigationApi::FinalizeWithAbortedNavigationError(
    ScriptState* script_state,
    NavigationApiNavigation* navigation) {
  if (ongoing_navigate_event_) {
    ongoing_navigate_event_->preventDefault();
    ongoing_navigate_event_ = nullptr;
  }

  ScriptValue error = ScriptValue::From(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                         "Navigation was aborted"));

  if (ongoing_navigation_signal_) {
    ongoing_navigation_signal_->SignalAbort(script_state, error);
    ongoing_navigation_signal_ = nullptr;
  }

  RejectPromisesAndFireNavigateErrorEvent(navigation, error);
}

int NavigationApi::GetIndexFor(NavigationHistoryEntry* entry) {
  const auto& it = keys_to_indices_.find(entry->key());
  if (it == keys_to_indices_.end() || entry != entries_[it->value])
    return -1;
  return it->value;
}

const AtomicString& NavigationApi::InterfaceName() const {
  return event_target_names::kNavigation;
}

void NavigationApi::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(entries_);
  visitor->Trace(transition_);
  visitor->Trace(ongoing_navigation_);
  visitor->Trace(upcoming_traversals_);
  visitor->Trace(upcoming_non_traversal_navigation_);
  visitor->Trace(ongoing_navigate_event_);
  visitor->Trace(ongoing_navigation_signal_);
}

}  // namespace blink
