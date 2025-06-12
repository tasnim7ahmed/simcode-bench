/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Wi-Fi Network with One AP and Two STA Nodes
 * ns-3 simulation with NetAnim visualization
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeWifiNetAnimExample");

int
main(int argc, char *argv[])
{
    // Enable logging for debugging (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 10.0; // seconds

    // 1. Create nodes: 1 AP and 2 STAs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // 2. Set up Wi-Fi PHY and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // 3. Set Wi-Fi standard and parameters
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // 4. Set up MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-netanim");

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // 5. Set mobility models
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // STA 0
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));   // STA 1
    positionAlloc->Add(Vector(10.0, 10.0, 0.0));  // AP

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Assign positions
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // 6. Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // 7. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    address.Assign(apDevice);

    // 8. Applications: UdpEcho from STA 0 to STA 1
    uint16_t port = 9;

    // Install UdpEchoServer on STA 1
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Install UdpEchoClient on STA 0
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5u));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024u));
    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // 9. NetAnim setup
    AnimationInterface anim("three-node-wifi-netanim.xml");

    // Optionally: set node descriptions and colors for easier visualization
    anim.SetConstantPosition(wifiStaNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(wifiStaNodes.Get(1), 20.0, 0.0);
    anim.SetConstantPosition(wifiApNode.Get(0), 10.0, 10.0);

    anim.UpdateNodeDescription(wifiStaNodes.Get(0), "STA 0");
    anim.UpdateNodeDescription(wifiStaNodes.Get(1), "STA 1");
    anim.UpdateNodeDescription(wifiApNode.Get(0), "AP");

    anim.UpdateNodeColor(wifiStaNodes.Get(0), 255, 0, 0);   // Red
    anim.UpdateNodeColor(wifiStaNodes.Get(1), 0, 255, 0);   // Green
    anim.UpdateNodeColor(wifiApNode.Get(0), 0, 0, 255);     // Blue

    // 10. Start simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}