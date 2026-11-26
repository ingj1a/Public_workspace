#include "tsf_edit.hpp"
#include <new>
#include <utility>

namespace nativeime::win {
namespace {
class Edit final : public ITfEditSession {
public:
    explicit Edit(const std::function<HRESULT(TfEditCookie)>& action) : action_(action) {}
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if(!object) return E_POINTER;
        *object=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITfEditSession) return E_NOINTERFACE;
        *object=static_cast<ITfEditSession*>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG count=InterlockedDecrement(&refs_);
        if(!count) delete this;
        return count;
    }
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
        try { return action_(cookie); } catch (...) { return E_FAIL; }
    }
private:
    LONG refs_=1;
    std::function<HRESULT(TfEditCookie)> action_;
};
}
HRESULT RunEdit(ITfContext* context, TfClientId client, const std::function<HRESULT(TfEditCookie)>& action) noexcept {
    if(!context || client==TF_CLIENTID_NULL || !action) return E_INVALIDARG;
    try {
        auto* edit=new Edit(action);
        HRESULT result=E_FAIL;
        const HRESULT request=context->RequestEditSession(client,edit,TF_ES_SYNC|TF_ES_READWRITE,&result);
        edit->Release();
        return request==S_OK ? result : (FAILED(request)?request:E_FAIL);
    } catch (...) { return E_OUTOFMEMORY; }
}
HRESULT InsertCommittedText(ITfContext* context, TfEditCookie cookie, const wchar_t* text, LONG length) noexcept {
    if(!context || !text || length<=0) return E_INVALIDARG;
    ITfInsertAtSelection* insert=nullptr;
    HRESULT hr=context->QueryInterface(IID_PPV_ARGS(&insert));
    if(SUCCEEDED(hr)) {
        hr=insert->InsertTextAtSelection(cookie,TF_IAS_NOQUERY,text,length,nullptr);
        insert->Release();
    }
    return hr;
}
}
