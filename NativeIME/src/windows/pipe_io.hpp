#pragma once
#include <windows.h>
namespace nativeime::win {
// One absolute deadline for a complete exchange. Overlapped operations are
// cancelled and drained before stack buffers are released.
inline bool Transfer(HANDLE h,void* buffer,DWORD count,bool write,ULONGLONG deadline) noexcept{
    auto* p=static_cast<unsigned char*>(buffer);
    while(count){
        const auto now=GetTickCount64();if(now>=deadline)return false;
        OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!ov.hEvent)return false;
        DWORD done=0;
        BOOL ok=write?WriteFile(h,p,count,&done,&ov):ReadFile(h,p,count,&done,&ov);
        if(!ok&&GetLastError()==ERROR_IO_PENDING){
            DWORD wait=WaitForSingleObject(ov.hEvent,static_cast<DWORD>(deadline-now));
            if(wait==WAIT_OBJECT_0)ok=GetOverlappedResult(h,&ov,&done,FALSE);
            else{CancelIoEx(h,&ov);GetOverlappedResult(h,&ov,&done,TRUE);ok=FALSE;}
        }
        CloseHandle(ov.hEvent);
        if(!ok||!done||done>count)return false;
        p+=done;count-=done;
    }
    return true;
}
}
