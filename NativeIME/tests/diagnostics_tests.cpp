#include "windows/diagnostics.hpp"
using namespace nativeime::win::diag;
int main(){
    // An explicit test session; no input data. Existing session must not be overwritten.
    Capture c;Writer w;
    if(!c.Start())return 1;
    Capture other;if(other.Start())return 2;
    w.Record(Event::Composition);w.Record(Event::IpcMilliseconds,12);
    w.RecordIpc(true,18);w.RecordCandidateLatency(9);w.RecordCandidateLatency(30);w.RecordCandidateLatency(151);
    auto a=c.Read();if(a[1]!=1||a[10]!=30||a[8]!=1||a[5]!=3||a[12]!=3||a[13]!=190||a[14]!=1||a[16]!=1||a[20]!=1)return 3;
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
