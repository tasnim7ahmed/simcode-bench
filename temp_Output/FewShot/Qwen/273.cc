#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 STAs and 1 AP
    NodeContainer staNodes;
    staNodes.Create(3);
    NodeContainer apNode;
    apNode.Create(1);

    // Install Wi-Fi devices using 802.11n standard in infrastructure mode
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // Default physical layer settings are fine
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up MAC layer for STAs and AP
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-infra")),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-infra")));
    NetDeviceContainer apDevices = wifiHelper.Install(wifiPhy, wifiMac, apNode);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNode);

    // Assign IP addresses from the 192.168.1.0/24 subnet
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    Ipv4InterfaceContainer apInterface;

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        Ipv4InterfaceContainer ifc = address.Assign(staDevices.Get(i));
        staInterfaces.Add(ifc);
    }
    apInterface = address.Assign(apDevices);

    // Mobility models for STAs with random movement
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(staNodes);

    // Stationary mobility model for AP
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);

    // Set up UDP Echo Server on the AP
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on one of the STAs (STA 0)
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}