#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ping6-application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ping6Simulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
    interfaces.SetForwarding(1, true);
    interfaces.SetDefaultRouteInAllNodes(1);

    Ping6Helper ping6;
    ping6.SetLocal(n0->GetId(), interfaces.GetAddress(0, 1));
    ping6.SetRemote(interfaces.GetAddress(1, 1));
    ping6.SetAttribute("Count", UintegerValue(10));
    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
    csma.EnablePcapAll("ping6");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}