#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkPing6");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    LogComponentEnable("WirelessSensorNetworkPing6", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devContainer = lrWpanHelper.Install(nodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(devContainer);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer interfaces;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    interfaces = ipv6.Assign(sixlowpanDevices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    uint32_t packetSize = 128;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping6;
    ping6.SetLocal(nodes.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress());
    ping6.SetRemote(interfaces.GetAddress(1, 1)); // Destination address of node 1
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("Size", UintegerValue(packetSize));
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));

    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wsn-ping6.tr");
    lrWpanHelper.EnableAsciiAll(stream);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}