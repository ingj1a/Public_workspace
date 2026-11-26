#include "windows/diagnostics.hpp"
using namespace nativeime::win::diag;
int main(){
    // An explicit test session; no input data. Existing session must not be overwritten.
    Capture c;Writer w;
    if(!c.Start())return 1;
    Capture other;if(other.Start())return 2;
    w.Record(Event::Composition);w.Record(Event::IpcMilliseconds,12);
    auto a=c.Read();if(a[1]!=1||a[10]!=12)return 3;
    if(!c.Stop())return 4;
    w.Record(Event::Commit);
    if(c.Read()[2]!=0)return 5;
    if(!c.Clear()||c.Read()[1]!=0)return 6;
    if(!c.Start())return 7;
    Sleep(1050);
    w.Record(Event::Commit);
    if(c.Read()[2]!=1||c.Read()[1]!=0)return 8;
    return c.Stop()?0:9;
}
