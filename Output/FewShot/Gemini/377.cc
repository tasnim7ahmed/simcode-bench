#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes: STA1, AP, STA2
    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNode;
    apNode.Create(1);

    // Configure Wi-Fi PHY layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC layer (STA)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    NqosWifiMacHelper macSta = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns3-wifi");
    macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, macSta, staNodes);

    // Configure Wi-Fi MAC layer (AP)
    NqosWifiMacHelper macAp = NqosWifiMacHelper::Default();
    macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, macAp, apNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNode);
    Ptr<Node> apNodePtr = apNode.Get(0);
    Ptr<MobilityModel> apMobility = apNodePtr->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(5.0, 0.0, 0.0));
    Ptr<Node> sta2NodePtr = staNodes.Get(1);
    Ptr<MobilityModel> sta2Mobility = sta2NodePtr->GetObject<MobilityModel>();
    sta2Mobility->SetPosition(Vector(10.0, 0.0, 0.0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterfaces = address.Assign(apDevices);

    // Create UDP server on STA2
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(staNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create UDP client on STA1, sending to AP
    UdpEchoClientHelper echoClient(apNodeInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up forwarding on AP
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}