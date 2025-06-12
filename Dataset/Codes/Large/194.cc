#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetworkExample");

int main(int argc, char *argv[])
{
    uint32_t nStas = 4; // Number of stations per AP
    double simulationTime = 10.0; // Simulation time in seconds

    // Create two access points and multiple stations
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nStas);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nStas);

    // Set up the Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi MAC layer (non-QoS)
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("network-1");
    Ssid ssid2 = Ssid("network-2");

    // Configure the APs
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Configure the stations for AP 1
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes1);

    // Configure the stations for AP 2
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, wifiStaNodes2);

    // Set up mobility models
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
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes1);
    stack.Install(wifiStaNodes2);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer ap1Interface, ap2Interface, sta1Interface, sta2Interface;

    address.SetBase("10.1.1.0", "255.255.255.0");
    ap1Interface = address.Assign(apDevices1);
    sta1Interface = address.Assign(staDevices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    ap2Interface = address.Assign(apDevices2);
    sta2Interface = address.Assign(staDevices2);

    // Set up UDP server on the access points
    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp1 = udpServer.Install(wifiApNodes.Get(0));
    ApplicationContainer serverApp2 = udpServer.Install(wifiApNodes.Get(1));
    serverApp1.Start(Seconds(1.0));
    serverApp2.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(simulationTime));
    serverApp2.Stop(Seconds(simulationTime));

    // Set up UDP clients on the stations
    UdpClientHelper udpClient1(ap1Interface.GetAddress(0), 9);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient1.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(1024));

    UdpClientHelper udpClient2(ap2Interface.GetAddress(0), 9);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1, clientApps2;
    clientApps1.Add(udpClient1.Install(wifiStaNodes1));
    clientApps2.Add(udpClient2.Install(wifiStaNodes2));

    clientApps1.Start(Seconds(2.0));
    clientApps2.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime));
    clientApps2.Stop(Seconds(simulationTime));

    // Enable packet tracing (ASCII and pcap files)
    phy.EnablePcap("wifi-network-ap1", apDevices1.Get(0));
    phy.EnablePcap("wifi-network-ap2", apDevices2.Get(0));
    phy.EnableAsciiAll(CreateObject<OutputStreamWrapper>("wifi-network.tr", std::ios::out));

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Log the total packets received by each AP
    Ptr<UdpServer> server1 = DynamicCast<UdpServer>(serverApp1.Get(0));
    Ptr<UdpServer> server2 = DynamicCast<UdpServer>(serverApp2.Get(0));
    NS_LOG_INFO("Total Packets Received by AP 1: " << server1->GetReceived());
    NS_LOG_INFO("Total Packets Received by AP 2: " << server2->GetReceived());

    Simulator::Destroy();
    return 0;
}

