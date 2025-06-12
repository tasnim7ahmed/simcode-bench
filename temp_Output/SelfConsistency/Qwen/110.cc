#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ping6Example");

int main(int argc, char *argv[])
{
    // Set up command line arguments
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging for the Ping application
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack with IPv6
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Create Ping application from n0 to n1
    PingHelper pingHelper(nodes.Get(0)->GetObject<Ipv6>()->GetAddress(1, 1).GetAddress());
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Size", UintegerValue(64));
    pingHelper.SetAttribute("Count", UintegerValue(5));

    ApplicationContainer apps = pingHelper.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Set up tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("ping6.tr");
    csma.EnableAsciiAll(stream);

    // Enable packet sniffing on all devices
    csma.EnablePcapAll("ping6", false);

    // Schedule simulation events
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}