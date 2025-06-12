#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Simulation parameters
    uint32_t numNodes = 10;
    double totalTime = 20.0;
    double txRange = 50.0; // meters
    uint16_t port = 9;
    double packetInterval = 1.0; // seconds

    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_DEBUG);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantVariable[Constant=2.0]"),
                              "Pause", StringValue("ns3::ConstantVariable[Constant=0.0]"),
                              "PositionAllocator", PointerValue(MobilityHelper::GetPositionAllocator()));
    mobility.Install(nodes);

    // Configure WiFi
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(aodv, 10);
    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install applications
    for (uint32_t i = 0; i < numNodes; ++i) {
        // UDP Echo Server on each node
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(totalTime));

        // UDP Echo Client on each node, sending to random neighbor
        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
        uint32_t destIndex;
        do {
            destIndex = static_cast<uint32_t>(rand->GetValue(0, numNodes));
        } while (destIndex == i); // Ensure it's not self

        UdpEchoClientHelper echoClient(interfaces.GetAddress(destIndex), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(totalTime / packetInterval)));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(totalTime));
    }

    // Run simulation
    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}