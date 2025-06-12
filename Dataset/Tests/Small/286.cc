#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvTest");

void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(5);
    NS_ASSERT_MSG(nodes.GetN() == 5, "Failed to create 5 nodes");
}

void TestWifiInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);
    
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);
    NS_ASSERT_MSG(devices.GetN() == 5, "Failed to install WiFi on all nodes");
}

void TestMobilityInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "PositionAllocator", StringValue("ns3::GridPositionAllocator"));
    mobility.Install(nodes);
    NS_ASSERT_MSG(nodes.Get(0)->GetObject<MobilityModel>() != nullptr, "Mobility model not installed");
}

void TestRoutingInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);
    
    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    Ipv4StaticRoutingHelper staticRouting;
    list.Add(staticRouting, 0);
    list.Add(aodv, 10);
    
    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);
    
    NS_ASSERT_MSG(nodes.Get(0)->GetObject<Ipv4>() != nullptr, "IPv4 stack not installed");
}

void TestUdpCommunication()
{
    NodeContainer nodes;
    nodes.Create(5);
    
    InternetStackHelper internet;
    internet.Install(nodes);
    
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));
    
    UdpClientHelper udpClient(interfaces.GetAddress(4), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));
    
    Simulator::Run();
    NS_ASSERT_MSG(udpServer.Get(0)->GetObject<UdpServer>()->GetReceived() > 0, "UDP packets not received");
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestWifiInstallation();
    TestMobilityInstallation();
    TestRoutingInstallation();
    TestUdpCommunication();
    NS_LOG_INFO("All tests passed successfully!");
    return 0;
}
