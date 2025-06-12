/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/wifi-module.h"
#include <iostream>
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Manet3NodesUdp");

uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;
std::vector<double> g_latencies;

void PacketSentCallback(Ptr<const Packet> packet)
{
    g_packetsSent++;
}

void PacketReceiveCallback(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        g_packetsReceived++;
        // Read the timestamp from packet
        Time sendTime;
        packet->PeekPacketTag(sendTime);
        Time now = Simulator::Now();
        double latency = (now - sendTime).GetMilliSeconds();
        g_latencies.push_back(latency);
    }
}

// Helper to add a timestamp tag to UDP packets (for latency measurement)
class TimestampTag : public Tag
{
public:
    static TypeId GetTypeId (void)
    {
        static TypeId tid = TypeId ("TimestampTag")
            .SetParent<Tag> ()
            .AddConstructor<TimestampTag> ();
        return tid;
    }

    TimestampTag () : m_sendTime () {}
    TimestampTag (Time sendTime) : m_sendTime (sendTime) {}

    void SetTimestamp (Time sendTime) { m_sendTime = sendTime; }
    Time GetTimestamp () const { return m_sendTime; }

    virtual uint32_t GetSerializedSize (void) const override { return sizeof(int64_t); }
    virtual void Serialize (TagBuffer i) const override
    {
        int64_t t = m_sendTime.GetNanoSeconds ();
        i.Write ((const uint8_t *)&t, sizeof(t));
    }
    virtual void Deserialize (TagBuffer i) override
    {
        int64_t t;
        i.Read ((uint8_t *)&t, sizeof(t));
        m_sendTime = NanoSeconds (t);
    }
    virtual void Print (std::ostream &os) const override
    {
        os << "Timestamp=" << m_sendTime.GetNanoSeconds ();
    }
private:
    Time m_sendTime;
};

void UdpCustomClientSendPacket(Ptr<Socket> socket, Address peer, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
    if (pktCount == 0)
        return;

    Ptr<Packet> packet = Create<Packet>(pktSize);
    TimestampTag tsTag(Simulator::Now());
    packet->AddPacketTag(tsTag);

    socket->SendTo(packet, 0, peer);
    g_packetsSent++;

    Simulator::Schedule(pktInterval, &UdpCustomClientSendPacket, socket, peer, pktSize, pktCount - 1, pktInterval);
}

void ReceivePacketWithTimestamp(Ptr<Socket> socket)
{
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(from)))
    {
        g_packetsReceived++;
        TimestampTag tsTag;
        if (packet->PeekPacketTag(tsTag))
        {
            Time now = Simulator::Now();
            double latency = (now - tsTag.GetTimestamp()).GetMilliSeconds();
            g_latencies.push_back(latency);
        }
    }
}

void TrackMobility(Ptr<Node> node, Ptr<OutputStreamWrapper> stream)
{
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    Vector pos = mob->GetPosition();
    *stream->GetStream() << Simulator::Now().GetSeconds()
                         << " Node" << node->GetId()
                         << " Position: (" << pos.x << "," << pos.y << "," << pos.z << ")\n";
    Simulator::Schedule(Seconds(1.0), &TrackMobility, node, stream);
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 3;
    double simTime = 30.0;
    uint32_t packetSize = 1024;
    uint32_t nPackets = 30;
    double packetInterval = 1.0;

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.AddValue("nPackets", "Number of packets", nPackets);
    cmd.AddValue("packetInterval", "Interval between packets (s)", packetInterval);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 200, 0, 200)),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=3.0|Max=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"));
    mobility.Install(nodes);

    // Internet stack + AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP traffic (custom sender, to support timestamp tag)
    // Node 0: sender; Node 2: receiver
    uint16_t port = 9;

    // Receiver socket
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(2), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(local);

    recvSocket->SetRecvCallback(MakeCallback(&ReceivePacketWithTimestamp));

    // Sender socket
    Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), tid);
    InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(2), port);

    Simulator::Schedule(Seconds(2.0), &UdpCustomClientSendPacket,
            sendSocket, remote, packetSize, nPackets, Seconds(packetInterval));

    // Mobility tracking
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> mobilityStream = ascii.CreateFileStream("manet-3nodes-mobility.txt");
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Simulator::Schedule(Seconds(0.0), &TrackMobility, nodes.Get(i), mobilityStream);
    }

    // NetAnim
    AnimationInterface anim("manet-3nodes.xml");
    anim.SetMaxPktsPerTraceFile(50000);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
        anim.UpdateNodeDescription (nodes.Get(i), "Node" + std::to_string(i)); // Optional

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Metrics
    double pdr = g_packetsSent > 0 ? (double)g_packetsReceived / g_packetsSent : 0.0;
    double avgLatency = 0;
    if (g_latencies.size() > 0)
    {
        double sum = 0;
        for (auto l : g_latencies) sum += l;
        avgLatency = sum / g_latencies.size();
    }
    std::cout << "Simulation completed:\n";
    std::cout << "  Packets sent: " << g_packetsSent << "\n";
    std::cout << "  Packets received: " << g_packetsReceived << "\n";
    std::cout << "  Packet Delivery Ratio: " << pdr * 100 << " %\n";
    std::cout << "  Average Latency: " << avgLatency << " ms\n";

    Simulator::Destroy();

    return 0;
}