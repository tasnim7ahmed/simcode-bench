#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <vector>
#include <map>
#include <iomanip> // For std::fixed, std::setprecision
#include <limits>  // For std::numeric_limits

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyDataset");

class TxTimeTag : public Tag {
public:
    static TypeId GetTypeId ();
    TypeId GetInstanceTypeId () const override;
    uint32_t GetSerializedSize () const override;
    void Serialize (Buffer::Iterator start) const override;
    void Deserialize (Buffer::Iterator start) override;
    void Print (std::ostream &os) const override;

    void SetTxTime (Time txTime);
    Time GetTxTime () const;

private:
    Time m_txTime;
};

TypeId TxTimeTag::GetTypeId () {
    static TypeId tid = TypeId ("ns3::TxTimeTag")
        .SetParent<Tag> ()
        .AddConstructor<TxTimeTag> ()
        .AddAttribute ("TxTime", "Time at which the packet was transmitted.",
                       EmptyAttributeValue (), MakeTimeAccessor (&TxTimeTag::m_txTime),
                       MakeTimeChecker ())
    ;
    return tid;
}

TypeId TxTimeTag::GetInstanceTypeId () const { return GetTypeId (); }
uint32_t TxTimeTag::GetSerializedSize () const { return 8; }
void TxTimeTag::Serialize (Buffer::Iterator i) const { i.WriteUsim (m_txTime.GetHighPrecisionNanoSeconds ()); }
void TxTimeTag::Deserialize (Buffer::Iterator i) { m_txTime = NanoSeconds (i.ReadUsim ()); }
void TxTimeTag::Print (std::ostream &os) const { os << "TxTime=" << m_txTime; }

void TxTimeTag::SetTxTime (Time txTime) { m_txTime = txTime; }
Time TxTimeTag::GetTxTime () const { return m_txTime; }

struct PacketRecord {
    uint32_t sourceNodeId;
    uint32_t destNodeId;
    uint32_t packetSize;
    double txTime;
    double rxTime;
};

std::vector<PacketRecord> g_packetRecords;
std::map<Ipv4Address, Ptr<Node>> g_ipToNodeMap;

class RingUdpServerApp : public Application {
public:
    static TypeId GetTypeId ();
    RingUdpServerApp ();
    ~RingUdpServerApp () override;

    void SetListenPort (uint16_t port);

private:
    void StartApplication () override;
    void StopApplication () override;

    void HandleRead (Ptr<Socket> socket);

    Ptr<Socket>     m_socket;
    uint16_t        m_port;
    bool            m_running;
};

TypeId RingUdpServerApp::GetTypeId () {
    static TypeId tid = TypeId ("ns3::RingUdpServerApp")
        .SetParent<Application> ()
        .AddConstructor<RingUdpServerApp> ()
        .AddAttribute ("Port", "Port on which to listen for incoming packets.",
                       UintegerValue (9), MakeUintegerAccessor (&RingUdpServerApp::m_port),
                       MakeUintegerChecker<uint16_t> ())
    ;
    return tid;
}

RingUdpServerApp::RingUdpServerApp () : m_socket (nullptr), m_port (0), m_running (false) {}
RingUdpServerApp::~RingUdpServerApp () {}

void RingUdpServerApp::SetListenPort (uint16_t port) { m_port = port; }

void RingUdpServerApp::StartApplication () {
    m_running = true;
    TypeId tid = SocketFactory::CreateTypeId("ns3::UdpSocketFactory");
    m_socket = SocketFactory::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    if (m_socket->Bind (local) == -1) {
        NS_FATAL_ERROR ("Failed to bind socket");
    }
    m_socket->SetRecvCallback (MakeCallback (&RingUdpServerApp::HandleRead, this));
}

void RingUdpServerApp::StopApplication () {
    m_running = false;
    if (m_socket) {
        m_socket->Close ();
        m_socket = nullptr;
    }
}

void RingUdpServerApp::HandleRead (Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from))) {
        if (packet->GetSize () > 0) {
            InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
            
            TxTimeTag tag;
            if (packet->RemovePacketTag (tag)) {
                PacketRecord record;
                record.txTime = tag.GetTxTime ().GetSeconds ();
                record.rxTime = Simulator::Now ().GetSeconds ();
                record.packetSize = packet->GetSize ();
                
                Ptr<Node> srcNode = nullptr;
                Ptr<Node> dstNode = GetNode();

                Ipv4Address srcIp = address.GetIpv4();
                auto it = g_ipToNodeMap.find(srcIp);
                if (it != g_ipToNodeMap.end()) {
                    srcNode = it->second;
                } else {
                    NS_LOG_ERROR("Source IP " << srcIp << " not found in g_ipToNodeMap. Assigning max uint32_t as Node ID.");
                    record.sourceNodeId = std::numeric_limits<uint32_t>::max();
                }
                
                if (srcNode) {
                    record.sourceNodeId = srcNode->GetId();
                } else {
                    record.sourceNodeId = std::numeric_limits<uint32_t>::max();
                }

                record.destNodeId = dstNode->GetId();
                
                g_packetRecords.push_back (record);
                NS_LOG_INFO ("At " << record.rxTime << "s received " << record.packetSize << " bytes from " << srcIp 
                             << " (Node " << record.sourceNodeId << ") at Node " << record.destNodeId 
                             << ". TxTime: " << record.txTime << " RxTime: " << record.rxTime);
            } else {
                NS_LOG_WARN ("Received packet without TxTimeTag from " << address.GetIpv4 ());
            }
        }
    }
}

class RingUdpClientApp : public Application {
public:
    static TypeId GetTypeId ();
    RingUdpClientApp ();
    ~RingUdpClientApp () override;

    void SetRemoteAddress (Ipv4Address ip, uint16_t port);
    void SetPacketSize (uint32_t size);
    void SetInterval (Time interval);
    void SetMaxPackets (uint32_t maxPackets);

private:
    void StartApplication () override;
    void StopApplication () override;

    void SendPacket ();

    Ptr<Socket>     m_socket;
    Ipv4Address     m_peerIp;
    uint16_t        m_peerPort;
    uint32_t        m_packetSize;
    Time            m_interval;
    uint32_t        m_maxPackets;
    uint32_t        m_packetsSent;
    EventId         m_sendEvent;
    bool            m_running;
};

TypeId RingUdpClientApp::GetTypeId () {
    static TypeId tid = TypeId ("ns3::RingUdpClientApp")
        .SetParent<Application> ()
        .AddConstructor<RingUdpClientApp> ()
        .AddAttribute ("RemoteAddress", "The IP address of the remote UDP echo server.",
                       Ipv4AddressValue (), MakeIpv4AddressAccessor (&RingUdpClientApp::m_peerIp),
                       MakeIpv4AddressChecker ())
        .AddAttribute ("RemotePort", "The port of the remote UDP echo server.",
                       UintegerValue (9), MakeUintegerAccessor (&RingUdpClientApp::m_peerPort),
                       MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("PacketSize", "Size of packets generated by the client.",
                       UintegerValue (1024), MakeUintegerAccessor (&RingUdpClientApp::m_packetSize),
                       MakeUintegerChecker<uint32_t> ())
        .AddAttribute ("Interval", "The time interval between packets.",
                       TimeValue (Seconds (1.0)), MakeTimeAccessor (&RingUdpClientApp::m_interval),
                       MakeTimeChecker ())
        .AddAttribute ("MaxPackets", "The maximum number of packets to send.",
                       UintegerValue (10), MakeUintegerAccessor (&RingUdpClientApp::m_maxPackets),
                       MakeUintegerChecker<uint32_t> ())
    ;
    return tid;
}

RingUdpClientApp::RingUdpClientApp () :
    m_socket (nullptr),
    m_peerIp (Ipv4Address::GetAny ()),
    m_peerPort (0),
    m_packetSize (0),
    m_interval (Time::Zero ()),
    m_maxPackets (0),
    m_packetsSent (0),
    m_sendEvent (),
    m_running (false) {}

RingUdpClientApp::~RingUdpClientApp () {}

void RingUdpClientApp::SetRemoteAddress (Ipv4Address ip, uint16_t port) {
    m_peerIp = ip;
    m_peerPort = port;
}

void RingUdpClientApp::SetPacketSize (uint32_t size) { m_packetSize = size; }
void RingUdpClientApp::SetInterval (Time interval) { m_interval = interval; }
void RingUdpClientApp::SetMaxPackets (uint32_t maxPackets) { m_maxPackets = maxPackets; }

void RingUdpClientApp::StartApplication () {
    m_running = true;
    m_packetsSent = 0;
    TypeId tid = SocketFactory::CreateTypeId ("ns3::UdpSocketFactory");
    m_socket = SocketFactory::CreateSocket (GetNode (), tid);
    m_socket->Bind ();

    m_socket->Connect (InetSocketAddress (m_peerIp, m_peerPort));

    SendPacket ();
}

void RingUdpClientApp::StopApplication () {
    m_running = false;
    if (m_sendEvent.IsRunning ()) {
        Simulator::Cancel (m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close ();
        m_socket = nullptr;
    }
}

void RingUdpClientApp::SendPacket () {
    NS_LOG_INFO ("Client at Node " << GetNode()->GetId() << " sending packet " << m_packetsSent + 1 << " to " << m_peerIp);
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    TxTimeTag txTag;
    txTag.SetTxTime (Simulator::Now ());
    packet->AddPacketTag (txTag);

    m_socket->Send (packet);

    m_packetsSent++;
    if (m_packetsSent < m_maxPackets) {
        m_sendEvent = Simulator::Schedule (m_interval, &RingUdpClientApp::SendPacket, this);
    }
}

int main (int argc, char *argv[]) {
    LogComponentEnable ("RingTopologyDataset", LOG_LEVEL_INFO);
    LogComponentEnable ("RingUdpClientApp", LOG_LEVEL_INFO);
    LogComponentEnable ("RingUdpServerApp", LOG_LEVEL_INFO);

    uint32_t numNodes = 4;
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    double interPacketInterval = 1.0;
    uint32_t maxPacketsPerClient = 5;

    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes in the ring topology (default 4)", numNodes);
    cmd.AddValue ("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue ("packetSize", "Size of UDP packets (bytes)", packetSize);
    cmd.AddValue ("interPacketInterval", "Time interval between packets (seconds)", interPacketInterval);
    cmd.AddValue ("maxPacketsPerClient", "Max packets each client sends", maxPacketsPerClient);
    cmd.Parse (argc, argv);

    if (numNodes != 4) {
        NS_LOG_WARN("This simulation is designed for 4 nodes in a ring. Setting numNodes to 4.");
        numNodes = 4;
    }

    NodeContainer nodes;
    nodes.Create (numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer p2pDevices[numNodes];
    Ipv4InterfaceContainer p2pInterfaces[numNodes];
    Ipv4AddressHelper address;

    for (uint32_t i = 0; i < numNodes; ++i) {
        p2pDevices[i] = p2p.Install (nodes.Get (i), nodes.Get ((i + 1) % numNodes));
        
        std::string ipBase = "10.1." + std::to_string(i+1) + ".0";
        address.SetBase (ipBase.c_str(), "255.255.255.0");
        p2pInterfaces[i] = address.Assign (p2pDevices[i]);

        g_ipToNodeMap[p2pInterfaces[i].GetAddress(0)] = nodes.Get(i);
        g_ipToNodeMap[p2pInterfaces[i].GetAddress(1)] = nodes.Get((i+1)%numNodes);
    }

    InternetStackHelper internet;
    internet.Install (nodes);

    uint16_t port = 9;

    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<RingUdpServerApp> serverApp = CreateObject<RingUdpServerApp> ();
        serverApp->SetListenPort (port);
        nodes.Get (i)->AddApplication (serverApp);
        serverApps.Add (serverApp);
    }
    serverApps.Start (Seconds (0.5));
    serverApps.Stop (Seconds (simTime + 1.5));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<RingUdpClientApp> clientApp = CreateObject<RingUdpClientApp> ();
        Ipv4Address destIp = p2pInterfaces[i].GetAddress (1);
        
        clientApp->SetRemoteAddress (destIp, port);
        clientApp->SetPacketSize (packetSize);
        clientApp->SetInterval (Seconds (interPacketInterval));
        clientApp->SetMaxPackets (maxPacketsPerClient);
        
        nodes.Get (i)->AddApplication (clientApp);
        clientApps.Add (clientApp);
    }
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Stop (Seconds (simTime + 2.0));
    Simulator::Run ();
    Simulator::Destroy ();

    std::ofstream outputFile ("ring_topology_dataset.csv");
    outputFile << "SourceNodeId,DestinationNodeId,PacketSize_bytes,TxTime_s,RxTime_s\n";
    for (const auto& record : g_packetRecords) {
        outputFile << record.sourceNodeId << ","
                   << record.destNodeId << ","
                   << record.packetSize << ","
                   << std::fixed << std::setprecision(9) << record.txTime << ","
                   << std::fixed << std::setprecision(9) << record.rxTime << "\n";
    }
    outputFile.close ();

    NS_LOG_INFO ("Simulation finished. Data saved to ring_topology_dataset.csv");

    return 0;
}