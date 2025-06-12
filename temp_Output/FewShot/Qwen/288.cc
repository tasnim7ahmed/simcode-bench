#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging if needed (optional)
    // LogComponentEnable("TcpServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("TcpClientApplication", LOG_LEVEL_INFO);

    // Create 3 nodes: AP, STA1 (server), STA2 (client)
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(2);

    NodeContainer allNodes = NodeContainer(apNode, staNodes);

    // Create and configure Wi-Fi in 802.11ac mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    // Set up PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up MAC layer
    Ssid ssid = Ssid("wifi-network");

    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Mobility model: random movement within a 50x50 area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNodes);

    // Stationary AP
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // TCP Server on STA1 (node 1)
    uint16_t serverPort = 5000;
    TcpServerHelper tcpServer(serverPort);
    ApplicationContainer serverApp = tcpServer.Install(staNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // TCP Client on STA2 (node 2) sending to STA1
    TcpClientHelper tcpClient(staInterfaces.GetAddress(0), serverPort);
    tcpClient.SetAttribute("MaxBytes", UintegerValue(0)); // continuous until simulation ends
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.0001))); // very frequent packets
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = tcpClient.Install(staNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("wifi_simulation", allNodes.Get(0)->GetId(), true, true);
    phy.EnablePcap("wifi_simulation", allNodes.Get(1)->GetId(), true, true);
    phy.EnablePcap("wifi_simulation", allNodes.Get(2)->GetId(), true, true);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}