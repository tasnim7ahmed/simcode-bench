#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnCsmaNetAnim");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void RxCallback(Ptr<const Packet> packet, const Address &address, uint64_t senderTimestamp)
{
    ++packetsReceived;
    double now = Simulator::Now().GetSeconds();
    double sentTime = senderTimestamp / 1e6; // microseconds to seconds
    totalDelay += (now - sentTime);
}

void UdpTraceRx(Ptr<const Packet> packet, const Address &from)
{
    TimestampTag tag;
    if (packet->PeekPacketTag(tag))
    {
        uint64_t sentTime = tag.GetTimestamp().GetMicroSeconds();
        RxCallback(packet, from, sentTime);
    }
}

class SenderApp : public Application
{
public:
    SenderApp() : m_socket(0), m_peer(), m_packetSize(50), m_nPackets(0), m_interval(Seconds(2.0)), m_sent(0) {}

    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
    {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_interval = interval;
    }

private:
    virtual void StartApplication()
    {
        m_socket->Connect(m_peer);
        SendPacket();
    }
    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }
    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        TimestampTag ts;
        ts.SetTimestamp(MicroSeconds(Simulator::Now().GetMicroSeconds()));
        packet->AddPacketTag(ts);

        m_socket->Send(packet);
        ++packetsSent;
        ++m_sent;

        if (m_sent < m_nPackets)
        {
            Simulator::Schedule(m_interval, &SenderApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_sent;
};

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double simTime = 20.0;
    uint32_t packetSize = 50;
    uint32_t numPacketsPerNode = 10;
    double sendInterval = 2.0;

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set positions in a grid for NetAnim
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(20, 20, 0));
    pos->Add(Vector(40, 20, 0));
    pos->Add(Vector(60, 20, 0));
    pos->Add(Vector(20, 40, 0));
    pos->Add(Vector(40, 40, 0));
    pos->Add(Vector(60, 40, 0));
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // The last node is base station
    uint16_t sinkPort = 8000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(numNodes - 1), sinkPort));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(numNodes - 1));
    sinkApps.Start(Seconds(0.));
    sinkApps.Stop(Seconds(simTime));

    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(numNodes - 1), UdpSocketFactory::GetTypeId());
    sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    sinkSocket->SetRecvCallback(MakeCallback(&UdpTraceRx));

    // Install sender apps on all other nodes.
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Ptr<SenderApp> app = CreateObject<SenderApp>();
        app->Setup(ns3UdpSocket, sinkAddress, packetSize, numPacketsPerNode, Seconds(sendInterval));
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0 + i));
        app->SetStopTime(Seconds(simTime));
    }

    // NetAnim setup
    AnimationInterface anim("wsn-csma-netanim.xml");
    anim.SetConstantPosition(nodes.Get(0), 20, 20);
    anim.SetConstantPosition(nodes.Get(1), 40, 20);
    anim.SetConstantPosition(nodes.Get(2), 60, 20);
    anim.SetConstantPosition(nodes.Get(3), 20, 40);
    anim.SetConstantPosition(nodes.Get(4), 40, 40);
    anim.SetConstantPosition(nodes.Get(5), 60, 40);

    anim.UpdateNodeDescription(nodes.Get(numNodes - 1), "BaseStation");
    anim.UpdateNodeColor(nodes.Get(numNodes - 1), 0, 255, 0);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    double pdr = (packetsSent == 0) ? 0 : ((double)packetsReceived / packetsSent) * 100.0;
    double avgDelay = (packetsReceived == 0) ? 0.0 : totalDelay / packetsReceived;

    std::cout << "Packets Sent: " << packetsSent << std::endl;
    std::cout << "Packets Received: " << packetsReceived << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    return 0;
}