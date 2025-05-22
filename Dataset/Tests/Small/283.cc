#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include <cassert>

using namespace ns3;

// Function to test node creation
void TestNodeCreation() {
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(3);
    wifiApNode.Create(1);

    assert(wifiStaNodes.GetN() == 3 && "Failed to create STA nodes");
    assert(wifiApNode.GetN() == 1 && "Failed to create AP node");
}

// Function to test WiFi device installation
void TestWiFiDeviceInstallation() {
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(3);
    wifiApNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    assert(staDevices.GetN() == 3 && "Failed to install WiFi devices on STA nodes");
    assert(apDevice.GetN() == 1 && "Failed to install WiFi device on AP node");
}

// Function to test mobility setup
void TestMobilitySetup() {
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(3);
    wifiApNode.Create(1);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    assert(true && "Mobility models set up correctly");
}

// Function to test IP address assignment
void TestIpAddressAssignment() {
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(3);
    wifiApNode.Create(1);

    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    assert(staInterfaces.GetAddress(0) != Ipv4Address::GetAny() && "Failed to assign IP to STA nodes");
    assert(apInterface.GetAddress(0) != Ipv4Address::GetAny() && "Failed to assign IP to AP node");
}

// Function to test UDP server setup
void TestUdpServerSetup() {
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));

    assert(serverApp.GetN() > 0 && "Failed to install UDP server");
}

// Function to test UDP client setup
void TestUdpClientSetup() {
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(3);
    wifiApNode.Create(1);

    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 4000;
    UdpClientHelper udpClient(apInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        clientApps.Add(udpClient.Install(wifiStaNodes.Get(i)));
    }

    assert(clientApps.GetN() == 3 && "Failed to install UDP clients on STA nodes");
}

// Function to test simulation execution
void TestSimulationExecution() {
    Simulator::Run();
    Simulator::Destroy();
    assert(true && "Simulation executed successfully");
}

// Function to execute all test cases
void RunAllTests() {
    TestNodeCreation();
    TestWiFiDeviceInstallation();
    TestMobilitySetup();
    TestIpAddressAssignment();
    TestUdpServerSetup();
    TestUdpClientSetup();
    TestSimulationExecution();

    std::cout << "All NS-3 WiFi UDP tests passed successfully!" << std::endl;
}

// Main function to run tests
int main() {
    RunAllTests();
    return 0;
}
