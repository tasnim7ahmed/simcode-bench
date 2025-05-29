#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdp");

class UdpClient : public Application {
public:
    UdpClient();
    virtual ~UdpClient();

    void Setup(Address address, uint32_t packetSize, uint32_t maxPackets, Time interPacketInterval);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendPacket(void);
    void SocketReceive(Ptr<Socket> socket);

    Address m_peerAddress;
    uint32_t m_packetSize;
    uint32_t m_maxPackets;
    Time m_interPacketInterval;
    Ptr<Socket> m_socket;
    uint32_t m_packetsSent;
    EventId m_sendEvent;
};

UdpClient::UdpClient() : m_peerAddress(), m_packetSize(0), m_maxPackets(0), m_interPacketInterval(), m_socket(nullptr), m_packetsSent(0), m_sendEvent() {}

UdpClient::~UdpClient() {
    m_socket = nullptr;
}

void UdpClient::Setup(Address address, uint32_t packetSize, uint32_t maxPackets, Time interPacketInterval) {
    m_peerAddress = address;
    m_packetSize = packetSize;
    m_maxPackets = maxPackets;
    m_interPacketInterval = interPacketInterval;
}

void UdpClient::StartApplication(void) {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    if (m_socket == nullptr) {
        NS_FATAL_ERROR("Failed to create socket");
    }
    m_socket->Bind();
    m_socket->Connect(m_peerAddress);
    m_socket->SetRecvCallback(MakeCallback(&UdpClient::SocketReceive, this));
    m_packetsSent = 0;
    SendPacket();
}

void UdpClient::StopApplication(void) {
    Simulator::Cancel(m_sendEvent);
    if (m_socket != nullptr) {
        m_socket->Close();
    }
}

void UdpClient::SendPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_maxPackets) {
        m_sendEvent = Simulator::Schedule(m_interPacketInterval, &UdpClient::SendPacket, this);
    }
}

void UdpClient::SocketReceive(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    NS_LOG_INFO("Client received " << packet->GetSize() << " bytes from " << from);
}

class UdpServer : public Application {
public:
    UdpServer();
    virtual ~UdpServer();

    void Setup(uint16_t port);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SocketReceive(Ptr<Socket> socket);
    void AcceptConnection(Ptr<Socket> socket, const Address& from);
    void ConnectionClosed(Ptr<Socket> socket);

    uint16_t m_port;
    Ptr<Socket> m_socket;
    std::list<Ptr<Socket>> m_peers;
};

UdpServer::UdpServer() : m_port(0), m_socket(nullptr), m_peers() {}

UdpServer::~UdpServer() {
    m_socket = nullptr;
    m_peers.clear();
}

void UdpServer::Setup(uint16_t port) {
    m_port = port;
}

void UdpServer::StartApplication(void) {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    if (m_socket == nullptr) {
        NS_FATAL_ERROR("Failed to create socket");
    }
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    if (m_socket->Bind(local) == -1) {
        NS_FATAL_ERROR("Failed to bind socket");
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::SocketReceive, this));
    m_socket->Listen();
}

void UdpServer::StopApplication(void) {
    if (m_socket != nullptr) {
        m_socket->Close();
    }
    for (auto peer : m_peers) {
        peer->Close();
    }
    m_peers.clear();
}

void UdpServer::SocketReceive(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    NS_LOG_INFO("Server received " << packet->GetSize() << " bytes from " << from);
}

void UdpServer::AcceptConnection(Ptr<Socket> socket, const Address& from) {
    NS_LOG_INFO("Accept connection from " << from);
    Ptr<Socket> peerSocket = socket->Accept();
    m_peers.push_back(peerSocket);
    peerSocket->SetRecvCallback(MakeCallback(&UdpServer::SocketReceive, this));
    peerSocket->SetCloseCallback(MakeCallback(&UdpServer::ConnectionClosed, this));
}

void UdpServer::ConnectionClosed(Ptr<Socket> socket) {
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (*it == socket) {
            m_peers.erase(it);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpServer serverApp;
    serverApp.Setup(port);
    ApplicationContainer serverApps;
    serverApps.Add(serverApp.Install(nodes.Get(1)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClient clientApp;
    clientApp.Setup(InetSocketAddress(interfaces.GetAddress(1), port), 1024, 10, Seconds(1));
    ApplicationContainer clientApps;
    clientApps.Add(clientApp.Install(nodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}