#pragma once
#include <windows.h>
#include <msctf.h>
#include <functional>

namespace nativeime::win {
// Only synchronous edits are permitted here. The callback owns no stack references
// beyond the request; TF_ES_SYNC rejects asynchronous scheduling.
HRESULT RunEdit(ITfContext* context, TfClientId client, const std::function<HRESULT(TfEditCookie)>& action) noexcept;
HRESULT InsertCommittedText(ITfContext* context, TfEditCookie cookie, const wchar_t* text, LONG length) noexcept;
}
