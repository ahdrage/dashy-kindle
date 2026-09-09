#include "Netatmo.h"
#include "NetatmoStore.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); ++failures; std::exit(1); } } while (0)
using namespace dashy;
static const time_t NOW = 1788984000;

struct MemoryStore : NetatmoStore {
    NetatmoConfig config{"id &+", "secret=+&", "refresh-old", ""};
    bool present = true, writable = true;
    int saves = 0;
    bool load(NetatmoConfig& value) override { value = config; return present; }
    bool save(const NetatmoConfig& value) override {
        ++saves;
        if (!writable) return false;
        config = value;
        return true;
    }
};
struct Transport : NetatmoTransport {
    std::vector<HttpResponse> responses;
    struct Request { std::string method, path, bearer, body; };
    std::vector<Request> requests;
    std::function<void()> onRequest;
    HttpResponse request(const std::string& method, const std::string& path,
                         const std::string& bearer, const std::string& body) override {
        requests.push_back({method,path,bearer,body});
        if (onRequest) onRequest();
        if (responses.empty()) { CHECK(false); return {-1,"",""}; }
        auto result = responses.front(); responses.erase(responses.begin()); return result;
    }
};
static HttpResponse token(const char* refresh="refresh-new", int expiry=10800) {
    return {200, std::string("{\"access_token\":\"access-secret\",\"refresh_token\":\"") + refresh +
            "\",\"expires_in\":" + std::to_string(expiry) + ",\"scope\":[\"read_station\"]}", ""};
}
static HttpResponse station() {
    return {200,R"({"body":{"devices":[{"_id":"main","dashboard_data":{"Temperature":21.3,"CO2":623,"time_utc":1788984000},"modules":[{"type":"NAModule4","dashboard_data":{"Temperature":25}},{"type":"NAModule1","dashboard_data":{"Temperature":14.2,"time_utc":1788984000}}]},{"_id":"other","dashboard_data":{"Temperature":18,"CO2":800,"time_utc":1788984000},"modules":[]}]}})",""};
}

static void runTests() {
    {
        MemoryStore store; Transport http; http.responses={token(),station(),station()};
        NetatmoClient client(http,store); client.begin();
        http.onRequest=[&] { if (http.requests.back().method=="GET") CHECK(store.config.refreshToken=="refresh-new"); };
        CHECK(client.poll(0,NOW,true));
        CHECK(http.requests.size()==2);
        CHECK(http.requests[0].path=="/oauth2/token");
        CHECK(http.requests[0].body=="grant_type=refresh_token&client_id=id%20%26%2B&client_secret=secret%3D%2B%26&refresh_token=refresh-old");
        CHECK(http.requests[1].bearer=="access-secret");
        auto view=client.view(NOW);
        CHECK(view.indoor=="21.3°C" && view.co2=="623 ppm" && view.outdoor=="14.2°C");
        CHECK(client.status()==NetatmoStatus::Live);
        CHECK(!client.poll(179,NOW+179,true));
        CHECK(client.poll(180,NOW+180,true));
        CHECK(http.requests.size()==3 && store.saves==1);
    }
    {
        MemoryStore store; Transport http; http.responses={token(),station()};
        NetatmoClient first(http,store); first.begin(); first.poll(0,NOW,true);
        Transport afterRestart; afterRestart.responses={token("refresh-latest"),station()};
        NetatmoClient second(afterRestart,store); second.begin(); second.poll(0,NOW,true);
        CHECK(afterRestart.requests[0].body.find("refresh_token=refresh-new")!=std::string::npos);
    }
    {
        MemoryStore store; Transport http; http.responses={token("refresh-new",120),station(),token("refresh-next"),station()};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true); client.poll(180,NOW+180,true);
        CHECK(http.requests.size()==4 && http.requests[2].method=="POST");
    }
    {
        MemoryStore store; Transport http;
        http.responses={token(),{401,"{}",""},token("retry-token"),{401,"{}",""}};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(http.requests.size()==4);
        CHECK(client.status()==NetatmoStatus::Authorization);
        CHECK(!client.poll(180,NOW+180,true));
    }
    {
        MemoryStore store; Transport http; http.responses={token(),station(),{-1,"",""}};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        client.poll(180,NOW+180,true);
        CHECK(client.view(NOW+180).indoor=="21.3°C");
        CHECK(client.status()==NetatmoStatus::Offline);
        CHECK(client.view(NOW+180).footer.find("NETTVERKSFEIL")!=std::string::npos);
    }
    {
        MemoryStore store; Transport http; http.responses={token(),station()};
        NetatmoClient client(http,store); client.begin();
        client.poll(0,NOW,false);
        CHECK(http.requests.empty() && client.status()==NetatmoStatus::Offline);
        CHECK(client.view(NOW).indoor=="--.-°C");
        client.poll(180,NOW,true);
        CHECK(client.status()==NetatmoStatus::Live);
        CHECK(client.view(NOW+1900).footer.find("ELDRE")!=std::string::npos);
    }
    {
        MemoryStore store; store.present=false; Transport http;
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(http.requests.empty() && client.status()==NetatmoStatus::Unconfigured);
        CHECK(client.view(NOW).footer.find("IKKE TILKOBLET")!=std::string::npos);
    }
    {
        MemoryStore store; Transport http; http.responses={token(),station()};
        NetatmoClient client(http,store); client.begin(); client.poll(0,0,true);
        CHECK(http.requests.empty() && client.status()==NetatmoStatus::Clock);
    }
    {
        MemoryStore store; Transport http; http.responses={{400,"{\"error\":\"invalid_grant\",\"secret\":\"do-not-log\"}",""}};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true); client.poll(999,NOW+999,true);
        CHECK(http.requests.size()==1 && client.status()==NetatmoStatus::Authorization);
        CHECK(client.view(NOW).footer.find("do-not-log")==std::string::npos);
    }
    {
        MemoryStore store; Transport http; http.responses={token(),{429,"{}","900"},station()};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(!client.poll(899,NOW+899,true));
        CHECK(client.poll(900,NOW+900,true));
        CHECK(http.requests.size()==3 && client.status()==NetatmoStatus::Live);
    }
    {
        MemoryStore store; store.writable=false; Transport http; http.responses={token(),station()};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(http.requests.size()==1 && client.status()==NetatmoStatus::Storage);
        store.writable=true; client.poll(180,NOW+180,true);
        CHECK(store.config.refreshToken=="refresh-new" && http.requests.size()==2);
    }
    {
        MemoryStore store; store.config.stationId="other"; Transport http; http.responses={token(),station()};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(http.requests[1].path=="/api/getstationsdata?device_id=other");
        CHECK(client.view(NOW).indoor=="18.0°C" && client.view(NOW).outdoor=="--.-°C");
    }
    {
        MemoryStore store; store.config.stationId="missing"; Transport http; http.responses={token(),station()};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(client.status()==NetatmoStatus::NoData);
    }
    for (const char* bad : {"not json", "{\"body\":{\"devices\":[]}}", "{\"body\":{\"devices\":[{\"dashboard_data\":{\"Temperature\":true,\"CO2\":\"620\"}}]}}"}) {
        MemoryStore store; Transport http; http.responses={token(),{200,bad,""}};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(client.status()==NetatmoStatus::NoData);
        CHECK(client.view(NOW).indoor=="--.-°C" && client.view(NOW).co2=="---- ppm");
    }
    {
        MemoryStore store; Transport http;
        http.responses={token(),{200,R"({"body":{"devices":[{"dashboard_data":{"Temperature":0,"CO2":0},"modules":[{"type":"NAModule4","dashboard_data":{"Temperature":-4.5}}]}]}})",""}};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(client.view(NOW).indoor=="0.0°C" && client.view(NOW).co2=="0 ppm" && client.view(NOW).outdoor=="-4.5°C");
    }
    for (const char* bad : {"{}", "{\"access_token\":\"x\",\"refresh_token\":\"y\",\"expires_in\":0}", "{\"access_token\":\"x\\r\\nInjected: yes\",\"refresh_token\":\"y\",\"expires_in\":1000}"}) {
        MemoryStore store; Transport http; http.responses={{200,bad,""}};
        NetatmoClient client(http,store); client.begin(); client.poll(0,NOW,true);
        CHECK(client.status()!=NetatmoStatus::Live && store.saves==0);
    }
}

static void fileStoreTests(const char* directory) {
    const auto path=std::string(directory)+"/netatmo.json";
    FileNetatmoStore store(path);
    NetatmoConfig config{"client", "private-value", "initial", "station"}, loaded;
    CHECK(store.save(config));
    struct stat st{}; CHECK(stat(path.c_str(),&st)==0 && (st.st_mode & 0777)==0600);
    CHECK(store.load(loaded) && loaded.refreshToken=="initial");
    config.refreshToken="rotated"; CHECK(store.save(config));
    CHECK(store.load(loaded) && loaded.refreshToken=="rotated");
    std::ofstream(path) << "{bad json";
    CHECK(!store.load(loaded));
    unlink(path.c_str());
    const auto victim=std::string(directory)+"/victim";
    std::ofstream(victim) << "preserve me";
    CHECK(symlink(victim.c_str(),path.c_str())==0);
    CHECK(!store.load(loaded));
    CHECK(!store.save(config));
    std::string line; std::ifstream(victim)>>line; CHECK(line=="preserve");
}
int main(int argc,char**argv) {
    if (argc!=2) return 2;
    runTests(); fileStoreTests(argv[1]);
    std::printf("Netatmo checks: %d failures\n",failures);
    return failures?1:0;
}
