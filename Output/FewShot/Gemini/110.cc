#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    QueueSize queueSize = QueueSize("20p");
    csma.SetQueue("ns3::DropTailQueue", "MaxPackets", QueueSizeValue(queueSize));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting(nodes.Get(0)->GetObject<Ipv6>());
    staticRouting->SetDefaultRoute(interfaces.GetAddress(1,1), 0);
    staticRouting = Ipv6RoutingHelper::GetRouting(nodes.Get(1)->GetObject<Ipv6>());
    staticRouting->SetDefaultRoute(interfaces.GetAddress(0,1), 0);

    uint32_t packetSize = 1024;
    uint32_t maxPackets = 5;
    Time interPacketInterval = Seconds(1.0);
    Ping6Helper ping6;

    ping6.SetRemote(interfaces.GetAddress(1, 1));
    ping6.SetAttribute("Verbose", BooleanValue(true));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));

    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    csma.EnablePcapAll("ping6", false);
    csma.EnableQueueDisc("pfifo", devices);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}