#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for AODV
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes: 1 GCS + multiple UAVs (e.g., 5 UAVs)
    NodeContainer gcsNode;
    gcsNode.Create(1);
    NodeContainer uavNodes;
    uavNodes.Create(5);
    NodeContainer allNodes = NodeContainer(gcsNode, uavNodes);

    // Set up Wi-Fi
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211a);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6Mbps"),
                                       "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiHelper.Install(wifiMac, allNodes);

    // Mobility model: 3D for UAVs and static for GCS
    MobilityHelper mobility;

    // GCS is static at ground level
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gcsNode);

    // UAVs with 3D constant velocity mobility
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                  "Z", StringValue("ns3::UniformRandomVariable[Min=100.0|Max=300.0]"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(uavNodes);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    Ipv4ListRoutingHelper listRoutingHelper;
    listRoutingHelper.Add(aodv, 10);

    InternetStackHelper stack;
    stack.SetRoutingHelper(listRoutingHelper);
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on GCS (node 0)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(gcsNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(60.0));

    // Set up UDP Echo Clients on each UAV sending to GCS
    for (uint32_t i = 0; i < uavNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port); // Send to GCS
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.05))); // Every 50ms
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(uavNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i)); // Staggered start times
        clientApp.Stop(Seconds(60.0));
    }

    // Run simulation
    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}