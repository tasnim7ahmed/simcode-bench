#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/tag.h"
#include "ns3/nstime.h"
#include "ns3/node.h" // For Node::GetId()

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

// Global CSV file stream for data output
std::ofstream g_outputCsvFile;

// 1. Custom Packet Tag: Carries source node ID, destination node ID, and transmission time
class PacketInfoTag : public Tag
{
public:
    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;
    virtual uint32_t GetSerializedSize() const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual void Deserialize(Buffer::Iterator start);
    virtual void Print(std::ostream &os) const;

    void SetSourceNodeId(uint32_t id);
    uint32_t GetSourceNodeId() const;

    void SetDestinationNodeId(uint32_t id);
    uint32_t GetDestinationNodeId() const;

    void SetTransmissionTime(Time time);
    Time GetTransmissionTime() const;

private:
    uint32_t m_sourceNodeId;
    uint32_t m_destinationNodeId;
    Time m_transmissionTime;
};

TypeId PacketInfoTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::PacketInfoTag")
        .SetParent<Tag>()
        .AddConstructor<PacketInfoTag>()
        .AddAttribute("SourceNodeId", "ID of the source node.",
                      UintegerValue(0), MakeUintegerAccessor(&PacketInfoTag::m_sourceNodeId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("DestinationNodeId", "ID of the destination node.",
                      UintegerValue(0), MakeUintegerAccessor(&PacketInfoTag::m_destinationNodeId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("TransmissionTime", "Time when the packet was sent.",
                      TimeValue(NanoSeconds(0)), MakeTimeAccessor(&PacketInfoTag::m_transmissionTime),
                      MakeTimeChecker());
    return tid;
}

TypeId PacketInfoTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t PacketInfoTag::GetSerializedSize() const
{
    // uint32_t for IDs, int64_t for Time (internal representation of ns-3 Time)
    return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int64_t); 
}

void PacketInfoTag::Serialize(Buffer::Iterator start) const
{
    start.WriteUinteger(m_sourceNodeId);
    start.WriteUinteger(m_destinationNodeId);
    start.WriteUinteger64(m_transmissionTime.GetNanoSeconds());
}

void PacketInfoTag::Deserialize(Buffer::Iterator start)
{
    m_sourceNodeId = start.ReadUinteger();
    m_destinationNodeId = start.ReadUinteger();
    m_transmissionTime = NanoSeconds(start.ReadUinteger64());
}

void PacketInfoTag::Print(std::ostream &os) const
{
    os << "PacketInfoTag: Src=" << m_sourceNodeId
       << ", Dst=" << m_destinationNodeId
       << ", TxTime=" << m_transmissionTime;
}

void PacketInfoTag::SetSourceNodeId(uint32_t id)
{
    m_sourceNodeId = id;
}

uint32_t PacketInfoTag::GetSourceNodeId() const
{
    return m_sourceNodeId;
}

void PacketInfoTag::SetDestinationNodeId(uint32_t id)
{
    m_destinationNodeId = id;
}

uint32_t PacketInfoTag::GetDestinationNodeId() const
{
    return m_destinationNodeId;
}

void PacketInfoTag::SetTransmissionTime(Time time)
{
    m_transmissionTime = time;
}

Time PacketInfoTag::GetTransmissionTime() const
{
    return m_transmissionTime;
}

// 2. Custom TCP Sender Application: Creates packets, adds PacketInfoTag, and sends
class CustomTcpSender : public Application
{
public:
    static TypeId GetTypeId();
    CustomTcpSender();
    virtual ~CustomTcpSender();

    void SetRemote(Ipv4Address ip, uint16_t port);
    void SetPacketSize(uint32_t packetSize);
    void SetInterval(Time interval);
    void SetCount(uint32_t count);
    void SetDestinationNodeId(uint32_t nodeId);

protected:
    virtual void StartApplication();
    virtual void StopApplication();

private:
    void SendPacket();
    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    Time m_interval;
    uint32_t m_count;
    uint32_t m_packetsSent;
    EventId m_sendEvent;
    uint32_t m_destinationNodeId; // The ID of the intended destination node
};

TypeId CustomTcpSender::GetTypeId()
{
    static TypeId tid = TypeId("ns3::CustomTcpSender")
        .SetParent<Application>()
        .AddConstructor<CustomTcpSender>()
        .AddAttribute("RemoteAddress", "The destination IpAddress",
                      Ipv4AddressValue("0.0.0.0"),
                      MakeIpv4AddressAccessor(&CustomTcpSender::m_peerAddress),
                      MakeIpv4AddressChecker())
        .AddAttribute("RemotePort", "The destination port",
                      UintegerValue(0),
                      MakeUintegerAccessor(&CustomTcpSender::m_peerPort),
                      MakeUintegerChecker<uint16_t>())
        .AddAttribute("PacketSize", "Size of packets generated",
                      UintegerValue(1024),
                      MakeUintegerAccessor(&CustomTcpSender::m_packetSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("Interval", "Time interval between packets",
                      TimeValue(Seconds(1.0)),
                      MakeTimeAccessor(&CustomTcpSender::m_interval),
                      MakeTimeChecker())
        .AddAttribute("Count", "Number of packets to send. Zero for unlimited.",
                      UintegerValue(0),
                      MakeUintegerAccessor(&CustomTcpSender::m_count),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("DestinationNodeId", "ID of the destination node",
                      UintegerValue(0),
                      MakeUintegerAccessor(&CustomTcpSender::m_destinationNodeId),
                      MakeUintegerChecker<uint32_t>());
    return tid;
}

CustomTcpSender::CustomTcpSender()
    : m_socket(nullptr),
      m_packetSize(1024),
      m_interval(Seconds(1.0)),
      m_count(0),
      m_packetsSent(0),
      m_destinationNodeId(0)
{
}

CustomTcpSender::~CustomTcpSender()
{
    m_socket = nullptr;
}

void CustomTcpSender::SetRemote(Ipv4Address ip, uint16_t port)
{
    m_peerAddress = ip;
    m_peerPort = port;
}

void CustomTcpSender::SetPacketSize(uint32_t packetSize)
{
    m_packetSize = packetSize;
}

void CustomTcpSender::SetInterval(Time interval)
{
    m_interval = interval;
}

void CustomTcpSender::SetCount(uint32_t count)
{
    m_count = count;
}

void CustomTcpSender::SetDestinationNodeId(uint32_t nodeId)
{
    m_destinationNodeId = nodeId;
}

void CustomTcpSender::StartApplication()
{
    m_packetsSent = 0;
    if (m_socket == nullptr)
    {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        m_socket->Connect(InetSocketAddress(m_peerAddress, m_peerPort));
        m_socket->SetConnectCallback(MakeCallback(&CustomTcpSender::ConnectionSucceeded, this),
                                     MakeCallback(&CustomTcpSender::ConnectionFailed, this));
    }
    // Schedule the first packet send after a small delay to allow connection setup
    Simulator::Schedule(Seconds(0.1), &CustomTcpSender::SendPacket, this);
}

void CustomTcpSender::StopApplication()
{
    if (m_socket != nullptr)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    Simulator::Cancel(m_sendEvent);
}

void CustomTcpSender::ConnectionSucceeded(Ptr<Socket> socket)
{
    // Connection established. Packets will be sent as scheduled.
}

void CustomTcpSender::ConnectionFailed(Ptr<Socket> socket)
{
    std::cerr << "Connection to " << m_peerAddress << ":" << m_peerPort << " failed!" << std::endl;
}

void CustomTcpSender::SendPacket()
{
    // Continue sending if not all packets are sent or count is 0 (unlimited)
    if (m_packetsSent < m_count || m_count == 0)
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        // Add custom tag to the packet
        PacketInfoTag infoTag;
        infoTag.SetSourceNodeId(GetNode()->GetId());      // Source node ID
        infoTag.SetDestinationNodeId(m_destinationNodeId); // Intended destination node ID
        infoTag.SetTransmissionTime(Simulator::Now());    // Transmission time
        packet->AddPacketTag(infoTag);

        m_socket->Send(packet);
        m_packetsSent++;

        // Schedule next packet send
        m_sendEvent = Simulator::Schedule(m_interval, &CustomTcpSender::SendPacket, this);
    }
    else
    {
        // All packets sent, close the socket
        m_socket->Close();
    }
}

// 3. Packet Reception Trace Callback: Extracts data from PacketInfoTag and writes to CSV
// This callback is bound to PacketSink::Rx trace source.
// The third argument (rxNode) is passed using MakeBoundCallback in main.
void PacketRxTrace(Ptr<const Packet> packet, const Address &address, Ptr<Node> rxNode)
{
    PacketInfoTag infoTag;
    if (packet->PeekPacketTag(infoTag)) // Try to retrieve our custom tag
    {
        uint32_t sourceNodeId = infoTag.GetSourceNodeId();
        uint32_t destNodeId = infoTag.GetDestinationNodeId();
        Time txTime = infoTag.GetTransmissionTime();
        Time rxTime = Simulator::Now();
        uint32_t packetSize = packet->GetSize();

        // Write the captured data to the CSV file
        // Sanity check: ensure the packet was intended for this receiver
        if (rxNode->GetId() == destNodeId)
        {
            g_outputCsvFile << sourceNodeId << ","
                            << destNodeId << ","
                            << packetSize << ","
                            << txTime.GetSeconds() << ","
                            << rxTime.GetSeconds() << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    // Configure default values for simulation parameters
    uint32_t nPeripheralNodes = 4;
    Time simulationTime = Seconds(10.0);
    uint32_t packetSize = 1024; // Bytes
    Time sendInterval = MilliSeconds(100);
    uint32_t numPacketsPerClient = 100; // 0 for unlimited

    // Allow command-line arguments to override default values
    CommandLine cmd(__FILE__);
    cmd.AddValue("nPeripheralNodes", "Number of peripheral nodes (excluding central node)", nPeripheralNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
    cmd.AddValue("sendInterval", "Time interval between packet sends (ms)", sendInterval);
    cmd.AddValue("numPacketsPerClient", "Number of packets sent by each client (0 for unlimited)", numPacketsPerClient);
    cmd.Parse(argc, argv);

    // Open CSV file and write header
    g_outputCsvFile.open("star_topology_data.csv");
    if (!g_outputCsvFile.is_open())
    {
        std::cerr << "Error: Could not open CSV file 'star_topology_data.csv'" << std::endl;
        return 1;
    }
    g_outputCsvFile << "SourceNode,DestinationNode,PacketSize,TxTimeSeconds,RxTimeSeconds" << std::endl;

    // 1. Create Nodes: 1 central node (node 0) + nPeripheralNodes (nodes 1 to nPeripheralNodes)
    NodeContainer nodes;
    nodes.Create(1 + nPeripheralNodes);

    // 2. Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 3. Create Point-to-Point Links: Central node connected to each peripheral node
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers(nPeripheralNodes);
    std::vector<Ipv4InterfaceContainer> interfaceContainers(nPeripheralNodes);
    Ipv4AddressHelper address;

    uint32_t baseNetwork = 10; // Starting third octet for IP address base
    for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
        // Connect central node (node 0) to peripheral node (node i+1)
        deviceContainers[i] = p2p.Install(nodes.Get(0), nodes.Get(i + 1));
        
        // Assign IP addresses for the current link
        std::string ipNetwork = "10." + std::to_string(baseNetwork + i) + ".1.0";
        address.SetBase(ipNetwork.c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);
    }

    // 4. Setup TCP Applications: CustomTcpSender on central node, PacketSink on peripheral nodes
    uint16_t port = 50000; // Base TCP port

    for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
        // Setup Packet Sink (receiver) on the peripheral node (nodes.Get(i + 1))
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port + i)); // Use unique port for each sink
        ApplicationContainer sinkApps = sink.Install(nodes.Get(i + 1));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(simulationTime);

        // Get pointer to the PacketSink application
        Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(sinkApps.Get(0));

        // Connect PacketSink's "Rx" trace source to our custom callback
        // MakeBoundCallback is used to pass the receiving node's pointer to the callback function
        sinkApp->TraceConnectWithoutContext("Rx", MakeBoundCallback(&PacketRxTrace, nodes.Get(i + 1)));

        // Setup Custom TCP Sender (client) on the central node (node 0)
        Ptr<CustomTcpSender> clientApp = CreateObject<CustomTcpSender>();
        // Remote address is the peripheral node's IP, remote port is the sink's port
        clientApp->SetRemote(interfaceContainers[i].GetAddress(1), port + i);
        clientApp->SetPacketSize(packetSize);
        clientApp->SetInterval(sendInterval);
        clientApp->SetCount(numPacketsPerClient);
        // Set the ID of the intended destination node for the custom tag
        clientApp->SetDestinationNodeId(nodes.Get(i + 1)->GetId());

        nodes.Get(0)->AddApplication(clientApp); // Install client app on central node
        
        // Stagger client start times slightly to avoid contention at the very beginning
        clientApp->SetStartTime(Seconds(1.0 + i * 0.1));
        clientApp->SetStopTime(simulationTime);
    }

    // Run Simulation
    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    // Close the CSV file
    g_outputCsvFile.close();

    return 0;
}