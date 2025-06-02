#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulationExample");

int main(int argc, char *argv[])
{
    double simulationTime = 10.0;

    // Create nodes for Access Points (AP) and Stations (STA)
    NodeContainer wifiApNodes;
    wifiApNodes.Create(3); // 3 Access Points

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(6); // 6 Stations

    // Create Wi-Fi Helper and configure the PHY and MAC layers
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiMacHelper macHelper;
    Ssid ssid1 = Ssid("AP1-SSID");
    Ssid ssid2 = Ssid("AP2-SSID");
    Ssid ssid3 = Ssid("AP3-SSID");

    // Install Wi-Fi devices on the APs
    NetDeviceContainer apDevices1, apDevices2, apDevices3;
    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(0));
    
    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(1));
    
    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(2));

    // Install Wi-Fi devices on the STAs
    NetDeviceContainer staDevices1, staDevices2, staDevices3;
    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    staDevices1 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(0));
    staDevices1.Add(wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(1)));

    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    staDevices2 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(2));
    staDevices2.Add(wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(3)));

    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3));
    staDevices3 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(4));
    staDevices3.Add(wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(5)));

    // Install mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);
    
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(wifiApNodes);
    internet.Install(wifiStaNodes);

    // Assign IP addresses to all devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = ipv4.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = ipv4.Assign(staDevices1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = ipv4.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = ipv4.Assign(staDevices2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces3 = ipv4.Assign(apDevices3);
    Ipv4InterfaceContainer staInterfaces3 = ipv4.Assign(staDevices3);

    // Set up UDP server on one of the stations
    uint16_t port = 9; // Discard port (just for testing)
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Set up UDP client to send traffic to the server
    UdpClientHelper client(staInterfaces1.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(5)); // Another station sends traffic
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // Enable NetAnim tracing
    AnimationInterface anim("wifi_netanim.xml");
    anim.SetConstantPosition(wifiApNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(wifiApNodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(wifiApNodes.Get(2), 20.0, 0.0);
    
    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

