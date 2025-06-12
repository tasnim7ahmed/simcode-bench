#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("LrWpanMac", LOG_LEVEL_ALL);
    LogComponentEnable("LrWpanCsmaCa", LOG_LEVEL_ALL);
    LogComponentEnable("LrWpanPhy", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
    LogComponentEnable("Ping6Application", LOG_LEVEL_ALL);

    // Create nodes (Sensor Nodes)
    NodeContainer nodes;
    nodes.Create(2);

    // Setup Mobility - Stationary
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Setup LR-WPAN (IEEE 802.15.4) channel and devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // Enable ASCII trace output for queue behavior
    AsciiTraceHelper asciiTraceHelper;
    lrWpanHelper.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wsn-ping6.tr"));

    // Install Internet stack with IPv6 support
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    // Assign specific IPv6 addresses
    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer interfaces;

    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    interfaces = ipv6.Assign(lrwpanDevices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);
    interfaces.SetForwarding(1, true);
    interfaces.SetDefaultRouteInAllNodes(1);

    // Configure Ping6 application (ICMPv6 echo requests)
    uint32_t packetSize = 100;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping6;
    ping6.SetLocal(nodes.Get(0), interfaces.GetAddress(0, 1));
    ping6.SetRemote(interfaces.GetAddress(1, 1));
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer app = ping6.Install(nodes.Get(0));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}