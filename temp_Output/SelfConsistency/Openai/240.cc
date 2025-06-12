#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMulticastSimulation");

void
ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Receiver " << 
                          socket->GetNode()->GetId() << " received packet of "
                          << packet->GetSize() << " bytes from "
                          << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }
}

int
main(int argc, char *argv[])
{
    uint32_t nReceivers = 3;
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1024; // bytes
    uint32_t nPackets = 20;
    double packetInterval = 0.5; // seconds

    CommandLine cmd;
    cmd.AddValue("nReceivers", "Number of receiver nodes", nReceivers);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nReceivers);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer allNodes;
    allNodes.Add(wifiApNode.Get(0));
    allNodes.Add(wifiStaNodes);

    // Wi-Fi channel and devices
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("multicast-wifi");

    // Configure STA (receiver) devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configure AP (also the sender; first node)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    for (uint32_t i = 0; i < nReceivers; ++i)
    {
        positionAlloc->Add(Vector(5.0 + i * 3, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Define multicast address
    Ipv4Address multicastGroup("224.1.2.3");
    uint16_t multicastPort = 5000;

    // Configure multicast routing (basic for Wi-Fi)
    Ipv4StaticRoutingHelper staticRoutingHelper;
    for (uint32_t i = 0; i < allNodes.GetN(); ++i)
    {
        Ptr<Node> node = allNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRouting = staticRoutingHelper.GetStaticRouting(ipv4);
        staticRouting->SetDefaultRoute(apInterfaces.GetAddress(0), 1);
    }

    // Create sockets and applications
    // Receivers: create UDP sockets, bind to multicast group/port
    for (uint32_t i = 0; i < nReceivers; ++i)
    {
        Ptr<Node> receiver = wifiStaNodes.Get(i);

        // Create socket and bind
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Socket> recvSink = Socket::CreateSocket(receiver, tid);

        InetSocketAddress local = InetSocketAddress(multicastGroup, multicastPort);
        recvSink->SetAllowBroadcast(true);
        recvSink->Bind(local);
        recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Join the multicast group
        Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4>();
        ipv4->SetMulticastLoopback(true); // For local delivery
        ipv4->JoinGroup(1, multicastGroup);
    }

    // Sender: UDP client to multicast address
    Ptr<Node> sender = wifiApNode.Get(0);

    // Use OnOffApplication as a simple UDP packet sender
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(multicastGroup, multicastPort));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(nPackets * packetSize));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer senderApp = onoff.Install(sender);
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(simulationTime - 1.0));

    // Set logging level of UdpClient and UdpServer (if needed)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}