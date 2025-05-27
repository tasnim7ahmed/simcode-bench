#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ping6Example");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nPackets = 1;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application containers to log if set to true", verbose);
    cmd.AddValue("nPackets", "Number of packets generated", nPackets);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Set IPv6 as default protocol
    GlobalRouteManager::PopulateRoutingTables();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Create Ping application
    V6PingHelper ping6(interfaces.GetAddress(1, 1));
    ping6.SetAttribute("Verbose", BooleanValue(verbose));
    ping6.SetAttribute("Count", UintegerValue(nPackets));

    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Tracing
    csma.EnablePcapAll("ping6", false);
    csma.EnableQueueDiscLogging("ping6", true);

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("ping6.tr");
    csma.EnableAsciiAll(stream);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}