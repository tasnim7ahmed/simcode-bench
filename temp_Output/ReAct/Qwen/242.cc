#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IoT6LoWPANSimulation");

int main(int argc, char *argv[]) {
    uint32_t numIotDevices = 5;
    double simulationTime = 10.0;
    double interval = 1.0;
    uint32_t packetSize = 128;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numIotDevices", "Number of IoT devices.", numIotDevices);
    cmd.AddValue("simulationTime", "Total simulation time (seconds).", simulationTime);
    cmd.AddValue("interval", "Interval between UDP packets (seconds).", interval);
    cmd.AddValue("packetSize", "Size of UDP packets (bytes).", packetSize);
    cmd.Parse(argc, argv);

    NodeContainer iotNodes;
    iotNodes.Create(numIotDevices);

    NodeContainer serverNode;
    serverNode.Create(1);

    // Create channel
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(NodeContainer(iotNodes, serverNode));
    lrWpanHelper.AssociateToPan(lrWpanDevices, 10);

    // Install IPv6 stack with 6LoWPAN
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(NodeContainer(iotNodes, serverNode));

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrWpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixlowpanDevices);
    interfaces.SetForwarding(1, true); // Enable routing on server

    // Set up static routing for the server
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> serverRouting = routingHelper.GetStaticRouting(serverNode.Get(0)->GetObject<Ipv6>());
    for (uint32_t i = 0; i < numIotDevices; ++i) {
        serverRouting->AddNetworkRouteTo(Ipv6Address::GetOnes(), Ipv6Prefix(64), interfaces.GetAddress(i, 0), 0, 1);
    }

    // Setup UDP echo server on central node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(serverNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Setup UDP echo clients on IoT nodes
    UdpEchoClientHelper echoClient(interfaces.GetAddress(numIotDevices, 0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(simulationTime / interval)));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numIotDevices; ++i) {
        clientApps.Add(echoClient.Install(iotNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer(iotNodes, serverNode));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}