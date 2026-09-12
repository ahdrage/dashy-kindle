#include "NightMode.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using namespace dashy;
static time_t at(int month,int day,int hour,int minute=0,int second=0) {
    std::tm tm{}; tm.tm_year=126; tm.tm_mon=month-1; tm.tm_mday=day;
    tm.tm_hour=hour; tm.tm_min=minute; tm.tm_sec=second; tm.tm_isdst=-1;
    return std::mktime(&tm);
}
struct Device : NightActions {
    bool quiet=true, prepared=true;
    int restores=0, preparations=0, pauses=0;
    uint32_t restoredSeconds=0;
    std::vector<uint32_t> sleeps;
    SleepResult result{true,62,60};
    bool pauseNetwork() override { ++pauses; return quiet; }
    bool prepareSleep() override { ++preparations; CHECK(quiet); return prepared; }
    SleepResult sleep(uint32_t seconds) override { CHECK(quiet && prepared); sleeps.push_back(seconds); return result; }
    void restore(uint32_t seconds) override { ++restores; restoredSeconds+=seconds; }
};
static void write(const std::string& p,const char* text) { std::ofstream(p)<<text; }
int main(int argc,char**argv) {
    if (argc!=2) return 2;
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1); tzset();
    CHECK(nightSleepSeconds(at(9,12,22,59,59))==0);
    CHECK(nightSleepSeconds(at(9,12,23))==8*3600);
    CHECK(nightSleepSeconds(at(9,13,0))==7*3600);
    CHECK(nightSleepSeconds(at(9,13,6,59,59))==1);
    CHECK(nightSleepSeconds(at(9,13,7))==0);
    CHECK(nightSleepSeconds(at(3,28,23))==7*3600); // spring clock change
    CHECK(nightSleepSeconds(at(10,24,23))==9*3600); // autumn clock change
    CHECK(nightSleepSeconds(at(12,31,23))==8*3600); // year rollover
    CHECK(nightSleepSeconds(0)==0);

    std::string path=std::string(argv[1])+"/night-mode.state";
    Device dev;
    NightMode night(dev,path);
    CHECK(!night.enabled());
    CHECK(!night.update(at(9,12,12),100));
    CHECK(dev.sleeps.empty()); // Let the first dashboard appear before testing.
    dev.quiet=false;
    CHECK(!night.update(at(9,12,12,0,11),111));
    CHECK(dev.preparations==0); // Do not cut a token rotation in half.
    dev.quiet=true;
    CHECK(!night.update(at(9,12,12,0,12),112));
    CHECK(dev.sleeps==std::vector<uint32_t>{60});
    CHECK(dev.restores==1 && dev.restoredSeconds==60 && night.enabled());
    NightMode reopened(dev,path);
    CHECK(reopened.enabled()); // The test is once, survives app updates/relaunches.
    CHECK(!reopened.update(at(9,12,22,59),200));
    dev.result={true,3602,3600}; // An early power-button wake at midnight.
    CHECK(reopened.update(at(9,12,23),260));
    CHECK(dev.sleeps.back()==8*3600);
    CHECK(dev.restores==1); // Keep the screen dark until morning.
    CHECK(reopened.update(at(9,13,0),262)); // Brief exit-gesture window after early wake.
    CHECK(dev.sleeps.size()==2);
    dev.result={true,7*3600,7*3600};
    CHECK(reopened.update(at(9,13,0,0,5),267));
    CHECK(dev.sleeps.back()==7*3600-5);
    CHECK(!reopened.update(at(9,13,7),269));
    CHECK(dev.restores==2 && dev.restoredSeconds==8*3600+60);

    write(path,"testing\n"); // Previous launch was interrupted during the test.
    Device interrupted;
    NightMode stopped(interrupted,path);
    stopped.update(at(9,12,12),0); stopped.update(at(9,12,23),500);
    CHECK(!stopped.enabled() && interrupted.sleeps.empty());
    CHECK(*stopped.notice());
    unlink(path.c_str());
    Device failed; failed.result={true,62,0}; // Merely waiting is not hardware suspend.
    NightMode noSuspend(failed,path);
    noSuspend.update(at(9,12,12),0); noSuspend.update(at(9,12,12),11);
    CHECK(!noSuspend.enabled() && failed.restores==1);
    noSuspend.update(at(9,12,23),100);
    CHECK(failed.sleeps.size()==1); // No repeated suspend loop on failure.

    write(path,"enabled\n");
    Device unsupported; unsupported.prepared=false;
    NightMode noHardware(unsupported,path);
    CHECK(!noHardware.update(at(9,12,23),0));
    CHECK(unsupported.sleeps.empty() && unsupported.restores==1 && !noHardware.enabled());
    write(path,"enabled\n");
    Device refused; refused.result={false,0,0};
    NightMode error(refused,path);
    CHECK(!error.update(at(9,12,23),0));
    CHECK(refused.restores==1 && !error.enabled());
    write(path,"enabled\n");
    Device late; late.quiet=false;
    NightMode morningRace(late,path);
    CHECK(!morningRace.update(at(9,13,6,59,59),0));
    CHECK(!morningRace.update(at(9,13,7),1));
    CHECK(late.restores==1); // Cancel a pending network pause if morning arrives first.
    unlink(path.c_str());
    CHECK(symlink("unrelated",path.c_str())==0);
    Device symlinked;
    NightMode wrongPath(symlinked,path);
    wrongPath.update(at(9,12,12),0); wrongPath.update(at(9,12,12),11);
    CHECK(symlinked.sleeps.empty());
    std::puts("Night schedule: boundaries, DST, one-time hardware test, safe sleep and recovery passed.");
}
