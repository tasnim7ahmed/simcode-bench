#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

using namespace ns3;

class CustomUdpServer : public Application {
public:
    CustomUdpServer() : m_socket(nullptr) {}
    virtual ~CustomUdpServer() {}

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("CustomUdpServer")
            .SetParent<Application>()
            .AddConstructor<CustomUdpServer>();
        return tid;
    }

    void Setup(Ptr<Socket> socket, uint16_t port) {
        m_socket = socket;
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
    }

protected:
    void StartApplication() override {
        // Nothing to do here for UDP server
    }

    void StopApplication() override {
        if (m_socket)
            m_socket->Close();
    }

private:
    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(65536, 0, from))) {
            std::cout << "Server received packet of size " << packet->GetSize()
                      << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
        }
    }

    Ptr<Socket> m_socket;
};

class CustomUdpClient : public Application {
public:
    CustomUdpClient() : m_socket(nullptr), m_interval(Seconds(1)), m_count(0), m_maxPackets(10) {}
    virtual ~CustomUdpClient() {}

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("CustomUdpClient")
            .SetParent<Application>()
            .AddConstructor<CustomUdpClient>()
            .AddAttribute("Interval", "Time between packets",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&CustomUdpClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("MaxPackets", "Total number of packets to send",
                          UintegerValue(10),
                          MakeUintegerAccessor(&CustomUdpClient::m_maxPackets),
                          MakeUintegerChecker<uint32_t>());
        return tid;
    }

    void Setup(Ptr<Socket> socket, Address address) {
        m_socket = socket;
        m_peerAddress = address;
    }

protected:
    void StartApplication() override {
        SendPacket();
    }

    void StopApplication() override {
        if (m_socket)
            m_socket->Close();
    }

private:
    void SendPacket() {
        if (m_count < m_maxPackets) {
            Ptr<Packet> packet = Create<Packet>(1024); // 1024-byte packet
            m_socket->SendTo(packet, 0, m_peerAddress);

            std::cout << "Client sent packet " << m_count << " at time "
                      << Simulator::Now().GetSeconds() << "s" << std::endl;

            m_count++;
            Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    Time m_interval;
    uint32_t m_count;
    uint32_t m_maxPackets;
};

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("CustomUdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("CustomUdpClient", LOG_LEVEL_INFO);

    // Create nodes: client and two servers
    NodeContainer nodes;
    nodes.Create(3); // 0: client, 1: server1, 2: server2

    // Connect all nodes via CSMA switch
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up custom UDP server applications
    uint16_t serverPort1 = 4000;
    uint16_t serverPort2 = 5000;

    // Server 1
    Ptr<Socket> sockServ1 = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
    Ptr<CustomUdpServer> serverApp1 = CreateObject<CustomUdpServer>();
    serverApp1->Setup(sockServ1, serverPort1);
    nodes.Get(1)->AddApplication(serverApp1);
    serverApp1->SetStartTime(Seconds(1.0));
    serverApp1->SetStopTime(Seconds(10.0));

    // Server 2
    Ptr<Socket> sockServ2 = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
    Ptr<CustomUdpServer> serverApp2 = CreateObject<CustomUdpServer>();
    serverApp2->Setup(sockServ2, serverPort2);
    nodes.Get(2)->AddApplication(serverApp2);
    serverApp2->SetStartTime(Seconds(1.0));
    serverApp2->SetStopTime(Seconds(10.0));

    // Set up custom UDP client application
    Ptr<Socket> sockClient = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

    // Client sends to both servers
    Ptr<CustomUdpClient> clientApp1 = CreateObject<CustomUdpClient>();
    InetSocketAddress remoteAddr1(interfaces.GetAddress(1), serverPort1);
    clientApp1->Setup(sockClient, remoteAddr1);
    nodes.Get(0)->AddApplication(clientApp1);
    clientApp1->SetStartTime(Seconds(2.0));
    clientApp1->SetStopTime(Seconds(10.0));

    Ptr<CustomUdpClient> clientApp2 = CreateObject<CustomUdpClient>();
    InetSocketAddress remoteAddr2(interfaces.GetAddress(2), serverPort2);
    clientApp2->Setup(sockClient, remoteAddr2);
    nodes.Get(0)->AddApplication(clientApp2);
    clientApp2->SetStartTime(Seconds(2.0));
    clientApp2->SetStopTime(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}