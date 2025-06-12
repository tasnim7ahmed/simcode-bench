#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ping6Simulation");

int main(int argc, char *argv[]) {
    uint32_t numPings = 5;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numPings", "Number of ping requests to send", numPings);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("QueueSize", QueueSizeValue(QueueSize("100p")));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    uint16_t port = 9;
    PingHelper ping(interfaces.GetAddress(1, 1));
    ping.SetAttribute("Count", UintegerValue(numPings));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Local", AddressValue(Address(interfaces.GetAddress(0, 1))));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(1.0 + numPings));

    AsciiTraceHelper ascii;
    if (tracing) {
        csma.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
        csma.EnablePcapAll("ping6", false);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}