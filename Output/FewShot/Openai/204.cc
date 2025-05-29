#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <vector>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSNGridCsmaSimulation");

static uint32_t packetsSent = 0;
static uint32_t packetsReceived = 0;
static double totalDelay = 0.0;

void
RxCallback(Ptr<const Packet> packet, const Address &from)
{
    packetsReceived++;
}

void
TxCallback(Ptr<const Packet> packet)
{
    packetsSent++;
}

void
RxWithDelayCallback(Ptr<const Packet> packet, const Address &from, const Time &delay)
{
    packetsReceived++;
    totalDelay += delay.GetMilliSeconds();
}

class WsnApp : public Application
{
public:
    WsnApp () {}
    virtual ~WsnApp () {m_socket = 0;}
    void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
    {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_interval = interval;
        m_sent = 0;
    }

private:
    virtual void StartApplication (void)
    {
        m_socket->Bind ();
        m_socket->Connect (m_peer);
        SendPacket();
    }

    virtual void StopApplication (void)
    {
        if (m_socket)
            m_socket->Close ();
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        m_socket->Send (packet);
        ++m_sent;
        packetsSent++;
        if (m_sent < m_nPackets)
        {
            Simulator::Schedule (m_interval, &WsnApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_sent;
};

void
UdpServerRx(std::string context, Ptr<const Packet> packet, const Address &address)
{
    packetsReceived++;
}

void
UdpServerRxWithDelay(std::string context, Ptr<const Packet> packet, const Address &address)
{
    static std::map<uint64_t, Time> sentTimes;
    uint64_t uid = packet->GetUid();
    if(sentTimes.find(uid) == sentTimes.end())
        return; // Should not happen in this minimal setup
    Time delay = Simulator::Now() - sentTimes[uid];
    totalDelay += delay.GetMilliSeconds();
    packetsReceived++;
}

void
PacketSentTracer(Ptr<const Packet> packet)
{
    packetsSent++;
}

void
LogResults()
{
    double pdr = packetsSent == 0 ? 0.0 : static_cast<double>(packetsReceived) / packetsSent * 100.0;
    double avgDelay = packetsReceived == 0 ? 0.0 : totalDelay / packetsReceived;
    std::cout << "Packes Sent: " << packetsSent << std::endl;
    std::cout << "Packets Received: " << packetsReceived << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " ms" << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    uint32_t packetSize = 64;
    double intervalSeconds = 1.0;
    uint32_t numPackets = 10;
    double simTime = intervalSeconds * numPackets + 2.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(numNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = csma.Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
        "MinX", DoubleValue (0.0),
        "MinY", DoubleValue (0.0),
        "DeltaX", DoubleValue (30.0),
        "DeltaY", DoubleValue (30.0),
        "GridWidth", UintegerValue (3),
        "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t udpPort = 4000;

    // Base station: node 0
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(simTime));

    // Sensor nodes: nodes 1-5
    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i) {
        UdpClientHelper client(interfaces.GetAddress(0), udpPort);
        client.SetAttribute("MaxPackets", UintegerValue(numPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(intervalSeconds)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime));
        clientApps.Add(app);
    }

    // Packet reception tracing
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&RxCallback));

    // NetAnim setup
    AnimationInterface anim("wsn-csma-netanim.xml");
    for (uint32_t i = 0; i < numNodes; ++i) {
        anim.UpdateNodeDescription(i, i==0?"BaseStation":"Sensor"); // Label nodes
        anim.UpdateNodeColor(i, i==0?0:0, i==0?255:255, 0); // BS is blue, others green
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    LogResults();

    Simulator::Destroy();
    return 0;
}