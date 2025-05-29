#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: AP and two STAs
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    // Configure Wi-Fi PHY layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure Wi-Fi MAC layer for AP
    Ssid ssid = Ssid("ns3-wifi");
    WifiMacHelper macAp;
    macAp.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice = wifi.Install(wifiPhy, macAp, apNode);

    // Configure Wi-Fi MAC layer for STAs
    WifiMacHelper macSta;
    macSta.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, macSta, staNodes);

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
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // UDP Echo Server on STA1
    UdpEchoServerHelper echoServer(8080);
    ApplicationContainer serverApp = echoServer.Install(staNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on STA2
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), 8080);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(staNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Run(Seconds(10.0));
    Simulator::Destroy();

    return 0;
}