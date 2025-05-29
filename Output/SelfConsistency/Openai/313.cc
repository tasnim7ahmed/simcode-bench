#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    double interval = 0.01; // seconds
    double simulationTime = 10.0; // seconds

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One AP

    // PHY and channel helpers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("simple-wifi");

    // Set up STA devices (non-QoS STA)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Set up AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;

    // Configure STA mobility: Random walk in a rectangle
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)));
    mobility.Install(wifiStaNodes);

    // AP is stationary at (25,25,0)
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);
    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(25.0, 25.0, 0.0));

    // Stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

    // UDP Server on STA 0 (will listen on port 9)
    uint16_t serverPort = 9;
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // UDP Client on STA 1 -> sends to STA 0 (its IP)
    UdpClientHelper client(staInterfaces.GetAddress(0), serverPort);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(1.0)); // Allow time for association
    clientApp.Stop(Seconds(simulationTime));

    // Enable pcap tracing if needed
    // phy.EnablePcap("simple-wifi", apDevices.Get(0));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    // Optionally print UDP server statistics
    Ptr<UdpServer> udpServer = serverApp.Get(0)->GetObject<UdpServer>();
    std::cout << "Total Packets Received by Server: " << udpServer->GetReceived() << std::endl;

    return 0;
}