#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("wifi-backward-compatibility-test");

void TestNodeCreation() {
    NodeContainer nodes;
    nodes.Create(2);
    NS_ASSERT_MSG(nodes.GetN() == 2, "Node creation failed");
    NS_LOG_INFO("TestNodeCreation passed.");
}

void TestMobilityAssignment() {
    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT_MSG(mobilityModel != nullptr, "Mobility assignment failed");
    NS_LOG_INFO("TestMobilityAssignment passed.");
}

void TestWifiInstallation() {
    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    NS_ASSERT_MSG(devices.GetN() == 2, "WiFi installation failed");
    NS_LOG_INFO("TestWifiInstallation passed.");
}

void TestDataTransmission() {
    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(5.0));

    UdpClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(5.0));

    Simulator::Run();
    NS_ASSERT_MSG(DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived() > 0, "No packets received");
    NS_LOG_INFO("TestDataTransmission passed.");
    Simulator::Destroy();
}

int main(int argc, char* argv[]) {
    LogComponentEnable("wifi-backward-compatibility-test", LOG_LEVEL_INFO);
    TestNodeCreation();
    TestMobilityAssignment();
    TestWifiInstallation();
    TestDataTransmission();
    NS_LOG_INFO("All tests passed.");
    return 0;
}
