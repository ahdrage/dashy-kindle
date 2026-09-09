// Uses the same TLS client, OAuth code and atomic token store as the Kindle.
#include "Netatmo.h"
#include "NetatmoHttp.h"
#include "NetatmoStore.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
int main(int argc,char**argv) {
    if (argc!=3) return 2;
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1); tzset();
    std::ifstream cert(argv[2]);
    std::string pem((std::istreambuf_iterator<char>(cert)),{});
    if (pem.empty() || pem.size()>16384) { std::fprintf(stderr,"Missing public TLS certificate\n"); return 2; }
    dashy::FileNetatmoStore store(argv[1]);
    dashy::NetatmoConfig config;
    if (!store.load(config)) { std::fprintf(stderr,"Netatmo credentials are missing or invalid; fill in the private file.\n"); return 2; }
    dashy::NetatmoHttps http(pem);
    dashy::NetatmoClient client(http,store);
    client.begin();
    client.poll(0,std::time(nullptr),true);
    const auto view=client.view(std::time(nullptr));
    if (client.status()!=dashy::NetatmoStatus::Live) {
        std::fprintf(stderr,"Netatmo check failed: %s\n",view.footer.c_str()); return 1;
    }
    std::printf("Netatmo connected: INNE %s | CO2 %s | UTE %s\n",view.indoor.c_str(),view.co2.c_str(),view.outdoor.c_str());
    std::puts("The current refresh token was saved privately. No credentials were printed.");
    return 0;
}
