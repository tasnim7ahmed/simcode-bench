#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ping6Example");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("PingApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    uint16_t numPings = 5;
    PingHelper pingHelper(interfaces.GetAddress(1, 1)); // node n1's IPv6 address
    pingHelper.SetAttribute("Count", UintegerValue(numPings));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Timeout", TimeValue(Seconds(0.1)));
    ApplicationContainer pingApp = pingHelper.Install(nodes.Get(0));
    pingApp.Start(Seconds(1.0));
    pingApp.Stop(Seconds(1.0 + (numPings * 1.1)));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
    csma.EnablePcapAll("ping6");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}