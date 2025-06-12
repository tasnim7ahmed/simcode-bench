#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvUdpExample");

const uint32_t numNodes = 10;
const double simTime = 20.0;
const double nodeSendInterval = 2.0;
const double areaSize = 100.0;
const uint16_t port = 4000;

// Helper to get neighbors within a transmission range
std::vector<uint32_t> GetNeighbors(Ptr<Node> node, NodeContainer &nodes, double txRange = 50.0)
{
    std::vector<uint32_t> neighbors;
    Ptr<MobilityModel> mobA = node->GetObject<MobilityModel>();
    for (uint32_t j = 0; j < nodes.GetN(); ++j)
    {
        if (node == nodes.Get(j))
            continue;
        Ptr<MobilityModel> mobB = nodes.Get(j)->GetObject<MobilityModel>();
        if (mobA->GetDistanceFrom(mobB) <= txRange)
        {
            neighbors.push_back(j);
        }
    }
    return neighbors;
}

// Application that periodically picks a random neighbor and sends a UDP packet
class RandomNeighborUdpSender : public Application
{
public:
    RandomNeighborUdpSender() : m_sendEvent(), m_socket(0), m_gen(std::random_device{}()) {}

    void Setup(Ptr<Node> node, NodeContainer nodes, double interval, uint16_t port)
    {
        m_node = node;
        m_nodes = nodes;
        m_interval = interval;
        m_port = port;
        m_rng = CreateObject<UniformRandomVariable>();
    }

    virtual void StartApplication()
    {
        m_socket = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());
        m_socket->Bind();
        ScheduleTx();
    }

    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
        Simulator::Cancel(m_sendEvent);
    }

private:
    void ScheduleTx()
    {
        m_sendEvent = Simulator::Schedule(Seconds(m_interval), &RandomNeighborUdpSender::SendPacket, this);
    }

    void SendPacket()
    {
        std::vector<uint32_t> neighbors = GetNeighbors(m_node, m_nodes);
        if (!neighbors.empty())
        {
            uint32_t idx = neighbors[m_rng->GetInteger(0, neighbors.size() - 1)];
            Ptr<Node> dstNode = m_nodes.Get(idx);
            Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
            Ipv4Address dstAddr = ipv4->GetAddress(1, 0).GetLocal();

            Ptr<Packet> packet = Create<Packet>(50);
            m_socket->SendTo(packet, 0, InetSocketAddress(dstAddr, m_port));
        }
        ScheduleTx();
    }

    EventId m_sendEvent;
    Ptr<Socket> m_socket;
    Ptr<Node> m_node;
    NodeContainer m_nodes;
    double m_interval;
    uint16_t m_port;
    Ptr<UniformRandomVariable> m_rng;
    std::mt19937 m_gen;
};

class UdpReceiver : public Application
{
public:
    UdpReceiver() : m_socket(0) {}
    void Setup(uint16_t port)
    {
        m_port = port;
    }
    virtual void StartApplication()
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
    }
    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }
private:
    void HandleRead(Ptr<Socket> socket)
    {
        while (Ptr<Packet> packet = socket->Recv())
        {
            // Packet received
        }
    }
    Ptr<Socket> m_socket;
    uint16_t m_port;
};

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wifi/802.11 setup (ad-hoc)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypoint
    MobilityHelper mobility;
    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
    pos.Set("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    pos.Set("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(pos.Create()));
    mobility.SetPositionAllocator(pos.Create());
    mobility.Install(nodes);

    // Internet/AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP apps
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<UdpReceiver> recvApp = CreateObject<UdpReceiver>();
        recvApp->Setup(port);
        nodes.Get(i)->AddApplication(recvApp);
        recvApp->SetStartTime(Seconds(0.0));
        recvApp->SetStopTime(Seconds(simTime));

        Ptr<RandomNeighborUdpSender> sndApp = CreateObject<RandomNeighborUdpSender>();
        sndApp->Setup(nodes.Get(i), nodes, nodeSendInterval, port);
        nodes.Get(i)->AddApplication(sndApp);
        sndApp->SetStartTime(Seconds(1.0));
        sndApp->SetStopTime(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}