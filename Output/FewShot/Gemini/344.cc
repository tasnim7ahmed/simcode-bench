#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));
    staDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(1)));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(2));

    // Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(staDevices);
    interfaces.Add(address.Assign(apDevices).Get(0));

    // TCP Server on Node 2
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));

    // TCP Clients on Node 0 and Node 1
    BulkSendHelper sourceHelper0("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    sourceHelper0.SetAttribute("MaxBytes", UintegerValue(5120)); // 10 packets * 512 bytes
    sourceHelper0.SetAttribute("SendSize", UintegerValue(512));
    ApplicationContainer sourceApp0 = sourceHelper0.Install(nodes.Get(0));
    sourceApp0.Start(Seconds(2.0));
    sourceApp0.Stop(Seconds(20.0));

    BulkSendHelper sourceHelper1("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    sourceHelper1.SetAttribute("MaxBytes", UintegerValue(5120)); // 10 packets * 512 bytes
    sourceHelper1.SetAttribute("SendSize", UintegerValue(512));
    ApplicationContainer sourceApp1 = sourceHelper1.Install(nodes.Get(1));
    sourceApp1.Start(Seconds(3.0));
    sourceApp1.Stop(Seconds(20.0));

    // Run Simulation
    Simulator::Stop(Seconds(20.0));

    // Animation Interface
    //AnimationInterface anim("wifi-animation.xml");
    //anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    //anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0);
    //anim.SetConstantPosition(nodes.Get(2), 0.0, 10.0);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}