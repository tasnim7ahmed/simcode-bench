#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMulticastSimulation");

int main(int argc, char *argv[]) {
    uint32_t nReceivers = 3;
    uint16_t port = 5000;
    double simulationTime = 10.0;

    // Enable UDP logging for receiver
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: 1 sender + n receivers
    NodeContainer senderNode;
    senderNode.Create(1);
    NodeContainer receiverNodes;
    receiverNodes.Create(nReceivers);

    NodeContainer allNodes;
    allNodes.Add(senderNode);
    allNodes.Add(receiverNodes);

    // Configure Wi-Fi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("multicast-wifi");

    // Set sender as STA
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer senderDev = wifi.Install(phy, mac, senderNode);

    // Set receivers as STA
    NetDeviceContainer receiverDevs = wifi.Install(phy, mac, receiverNodes);

    // Create AP (access point) to connect all STAs
    NodeContainer apNode;
    apNode.Create(1);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDev = wifi.Install(phy, mac, apNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(senderNode);
    mobility.Install(receiverNodes);
    mobility.Install(apNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);
    stack.Install(apNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer allDevs;
    allDevs.Add(senderDev);
    allDevs.Add(receiverDevs);
    allDevs.Add(apDev);

    Ipv4InterfaceContainer interfaces = address.Assign(allDevs);

    // Define multicast group address
    Ipv4Address multicastGroup("224.1.2.3");

    // Configure sender to send UDP packets to multicast group
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(multicastGroup, port)));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
    ApplicationContainer senderApp = onoff.Install(senderNode.Get(0));

    // Set up multicast route on sender and receivers
    Ipv4StaticRoutingHelper multicastRouting;
    Ptr<Node> sender = senderNode.Get(0);
    Ptr<NetDevice> senderNetDev = senderDev.Get(0)->GetObject<NetDevice>();
    Ptr<Ipv4> ipv4Sender = sender->GetObject<Ipv4>();
    multicastRouting.AddMulticastRoute(sender, 1, 1, multicastGroup);

    Ptr<Node> ap = apNode.Get(0);
    multicastRouting.AddMulticastRoute(ap, 1, 2, multicastGroup);

    for (uint32_t i = 0; i < receiverNodes.GetN(); ++i) {
        Ptr<Node> receiver = receiverNodes.Get(i);
        Ptr<Ipv4StaticRouting> receiverRouting = multicastRouting.GetStaticRouting(receiver->GetObject<Ipv4>());
        receiverRouting->SetDefaultMulticastRoute(1);
    }

    // Set up receivers as UDP sinks listening on the multicast group
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < receiverNodes.GetN(); ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer app = sinkHelper.Install(receiverNodes.Get(i));
        app.Start(Seconds(0.));
        app.Stop(Seconds(simulationTime));
        sinkApps.Add(app);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}