#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Test for node creation
void TestNodeCreation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 2 stations (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // 1 AP node

    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Failed to create 2 Wi-Fi station nodes.");
    NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create 1 Wi-Fi AP node.");
}

// Test for Wi-Fi device installation on the stations (clients) and access point (AP)
void TestWifiDeviceInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 2 stations (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // 1 AP node

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Failed to install Wi-Fi devices on stations.");
    NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on AP.");
}

// Test for mobility model installation
void TestMobilityModelInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 2 stations (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // 1 AP node

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Check if mobility models were installed
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.Get(0)->GetObject<MobilityModel>(), nullptr, "Mobility model installation failed on station 0.");
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.Get(1)->GetObject<MobilityModel>(), nullptr, "Mobility model installation failed on station 1.");
    NS_TEST_ASSERT_MSG_EQ(wifiApNode.Get(0)->GetObject<MobilityModel>(), nullptr, "Mobility model installation failed on AP.");
}

// Test for IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 2 stations (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // 1 AP node

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    NS_TEST_ASSERT_MSG_EQ(staInterfaces.GetN(), 2, "IP address assignment failed for stations.");
    NS_TEST_ASSERT_MSG_EQ(apInterface.GetN(), 1, "IP address assignment failed for AP.");
}

// Test for UDP server and client application installation
void TestUdpApplicationInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 2 stations (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // 1 AP node

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(staInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server application.");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client application.");
}

// Test for packet tracing (Pcap)
void TestPacketTrace()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 2 stations (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // 1 AP node

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(staInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    phy.EnablePcap("wifi-udp", apDevice.Get(0));

    // Check if pcap file is created
    NS_TEST_ASSERT_MSG_EQ(system("ls wifi-udp-0-0.pcap"), 0, "Pcap file was not created.");
}

// Main function to execute all the tests
int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestMobilityModelInstallation();
    TestIpAddressAssignment();
    TestUdpApplicationInstallation();
    TestPacketTrace();

    return 0;
}
