#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"

int main(int argc, char *argv[])
{
    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure random movement for both nodes using a random walk mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Time", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100))); // Set a 200x200m area

    // Set initial positions for clarity
    nodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0)); // Slightly offset
    mobility.Install(nodes);

    // 3. Configure WiFi for MANET
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b); // Use 802.11b for 1 Mbps DSSS rates

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel"); // Simple propagation loss model
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set 1 Mbps link speed using ConstantRateWifiManager
    // DsssRate1Mbps is the 1 Mbps mode for 802.11b
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate1Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 4. Install Internet Stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 5. Configure UDP applications
    uint16_t port = 9; // Port 9 for communication

    // Server on Node 1 (interfaces.GetAddress(1) is the IP of node 1's interface)
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    // Client on Node 0 (interfaces.GetAddress(0) is the IP of node 0's interface)
    // Client sends to server (Node 1's IP)
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));      // Client sends 100 packets
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));   // with 0.1 seconds interval
    client.SetAttribute("PacketSize", UintegerValue(512));      // each packet 512 bytes
    
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client after 1 second to allow routing to settle
    clientApps.Stop(Seconds(20.0));

    // 6. Simulation configuration
    Simulator::Stop(Seconds(20.0)); // Simulation runs for 20 seconds

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}