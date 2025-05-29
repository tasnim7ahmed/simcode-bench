#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

std::ofstream csvFile;

// Data structure to save information about each packet
struct PacketLogEntry {
    uint32_t packetId;
    uint32_t srcNode;
    uint32_t dstNode;
    uint32_t packetSize;
    double txTime;
    double rxTime;
};

std::map<uint64_t, PacketLogEntry> packetLogs;

// Packet transmit callback
void TxCallback(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface, Ipv4Address addr, uint16_t port, uint8_t protocol, Ptr<Node> node, uint32_t dstNode) {
    uint64_t uid = packet->GetUid();
    PacketLogEntry entry;
    entry.packetId = uid;
    entry.srcNode = node->GetId();
    entry.dstNode = dstNode;
    entry.packetSize = packet->GetSize();
    entry.txTime = Simulator::Now().GetSeconds();
    entry.rxTime = -1.0;
    packetLogs[uid] = entry;
}

// Packet receive callback
void RxCallback(Ptr<const Packet> packet, const Address &address, Ptr<Node> node) {
    uint64_t uid = packet->GetUid();
    auto it = packetLogs.find(uid);
    if (it != packetLogs.end()) {
        it->second.rxTime = Simulator::Now().GetSeconds();
        // Write to CSV
        csvFile << it->second.srcNode << ","
                << it->second.dstNode << ","
                << it->second.packetSize << ","
                << it->second.txTime << ","
                << it->second.rxTime << std::endl;
        packetLogs.erase(it);
    }
}

// Custom UDP client that also triggers the TxCallback
class UdpRingClient : public Application {
public:
    UdpRingClient() : m_socket(0), m_sendEvent(), m_running(false), m_packetsSent(0) {}

    void Setup(Address address, uint32_t packetSize, uint32_t numPackets, Time interval, uint32_t dstNodeId) {
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = numPackets;
        m_interval = interval;
        m_dstNodeId = dstNodeId;
    }

    virtual void StartApplication() override {
        m_running = true;
        m_packetsSent = 0;
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(m_peer);
        SendPacket();
    }

    virtual void StopApplication() override {
        m_running = false;
        if (m_sendEvent.IsRunning())
            Simulator::Cancel(m_sendEvent);
        if (m_socket)
            m_socket->Close();
    }

private:
    void SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);
        // Log tx event
        TxCallback(packet, nullptr, 0, Ipv4Address(), 0, 17, GetNode(), m_dstNodeId);
        ++m_packetsSent;
        if (m_packetsSent < m_nPackets && m_running) {
            m_sendEvent = Simulator::Schedule(m_interval, &UdpRingClient::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_packetsSent;
    bool m_running;
    EventId m_sendEvent;
    uint32_t m_dstNodeId;
};

int main(int argc, char *argv[]) {
    // Prepare CSV
    csvFile.open("ring_topology_packets.csv");
    csvFile << "Source,Destination,PacketSize,TxTime,RxTime" << std::endl;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create a ring using point-to-point links
    std::vector<NetDeviceContainer> devs(4);
    std::vector<Ipv4InterfaceContainer> ifaces(4);
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t next = (i + 1) % 4;
        devs[i] = p2p.Install(nodes.Get(i), nodes.Get(next));
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        ifaces[i] = address.Assign(devs[i]);
    }

    uint32_t packetSize = 512;
    uint32_t numPackets = 5;
    Time pktInterval = Seconds(1.0);
    uint16_t port = 9000;

    // Install applications
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t next = (i + 1) % 4;
        // Set up UDP server (sink)
        Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(next), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        sinkSocket->Bind(local);
        sinkSocket->SetRecvCallback(MakeBoundCallback(&RxCallback, nodes.Get(next)));
        // Install custom UDP client
        Ptr<UdpRingClient> client = CreateObject<UdpRingClient>();
        Address peerAddress = InetSocketAddress(ifaces[i].GetAddress(1), port); // neighbor's address
        client->Setup(peerAddress, packetSize, numPackets, pktInterval, next);
        nodes.Get(i)->AddApplication(client);
        client->SetStartTime(Seconds(2.0 + i)); // staggered starts
        client->SetStopTime(Seconds(10.0));
    }

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    csvFile.close();
    return 0;
}