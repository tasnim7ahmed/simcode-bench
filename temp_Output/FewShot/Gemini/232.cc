#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

    // Create nodes: 1 server, 3 clients
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(3);

    // Create the Wi-Fi network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer clientDevices = wifi.Install(phy, mac, clientNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(1.0)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, serverNode);

    // Mobility model (static for simplicity)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clientNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(serverNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(clientNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);

    // Create TCP server application
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(serverInterfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create TCP client applications
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024)); // Small amount of data
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));

        ApplicationContainer clientApp = onoff.Install(clientNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.5));
        clientApp.Stop(Seconds(9.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Animation Interface
    AnimationInterface anim("wifi-tcp-simple.xml");
    anim.SetConstantPosition(serverNode.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(clientNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(clientNodes.Get(1), 0.0, 10.0);
    anim.SetConstantPosition(clientNodes.Get(2), 0.0, 20.0);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}