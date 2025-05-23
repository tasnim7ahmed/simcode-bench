#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("UdpForwarder");

class UdpForwarder : public Application {
public:
    static TypeId GetTypeId();
    UdpForwarder();
    ~UdpForwarder() override;

    void SetListeningPort(uint16_t port);
    void SetTarget(Ipv4Address addr, uint16_t port);

protected:
    void DoInitialize() override;
    void DoDispose() override;
    void StartApplication() override;
    void StopApplication() override;

private:
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_recvSocket;
    Ptr<Socket> m_sendSocket;

    uint16_t m_recvPort;
    Ipv4Address m_targetAddress;
    uint16_t m_targetPort;
};

TypeId UdpForwarder::GetTypeId() {
    static TypeId tid = TypeId("ns3::UdpForwarder")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<UdpForwarder>()
        .AddAttribute("ListeningPort", "Port to listen on for incoming client packets.",
                      UintegerValue(9),
                      MakeUintegerAccessor(&UdpForwarder::m_recvPort),
                      MakeUintegerChecker<uint16_t>())
        .AddAttribute("TargetAddress", "IPv4 address of the target server.",
                      Ipv4AddressValue(),
                      MakeIpv4AddressAccessor(&UdpForwarder::m_targetAddress),
                      MakeIpv4AddressChecker())
        .AddAttribute("TargetPort", "Port on the target server to send packets to.",
                      UintegerValue(10),
                      MakeUintegerAccessor(&UdpForwarder::m_targetPort),
                      MakeUintegerChecker<uint16_t>());
    return tid;
}

UdpForwarder::UdpForwarder() : m_recvSocket(nullptr), m_sendSocket(nullptr),
                               m_recvPort(9), m_targetAddress(), m_targetPort(10) {}

UdpForwarder::~UdpForwarder() {}

void UdpForwarder::SetListeningPort(uint16_t port) {
    m_recvPort = port;
}

void UdpForwarder::SetTarget(Ipv4Address addr, uint16_t port) {
    m_targetAddress = addr;
    m_targetPort = port;
}

void UdpForwarder::DoInitialize() {
    Application::DoInitialize();
}

void UdpForwarder::DoDispose() {
    if (m_recvSocket != nullptr) {
        m_recvSocket->Close();
        m_recvSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    if (m_sendSocket != nullptr) {
        m_sendSocket->Close();
    }
    Application::DoDispose();
}

void UdpForwarder::StartApplication() {
    NS_LOG_FUNCTION(this);

    if (m_recvSocket == nullptr) {
        m_recvSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress localAddress = InetSocketAddress(Ipv4Address::GetAny(), m_recvPort);
        if (m_recvSocket->Bind(localAddress) == -1) {
            NS_LOG_ERROR("Failed to bind recv socket to " << localAddress);
            return;
        }
        m_recvSocket->SetRecvCallback(MakeCallback(&UdpForwarder::HandleRead, this));
        NS_LOG_INFO("UdpForwarder " << GetNode()->GetId() << " listening on " << localAddress);
    }

    if (m_sendSocket == nullptr) {
        m_sendSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    }
}

void UdpForwarder::StopApplication() {
    NS_LOG_FUNCTION(this);
    DoDispose();
}

void UdpForwarder::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address fromAddress;
    while ((packet = socket->RecvFrom(fromAddress))) {
        if (InetSocketAddress::IsMatchingType(fromAddress)) {
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s UdpForwarder received "
                                   << packet->GetSize() << " bytes from "
                                   << InetSocketAddress::ConvertFrom(fromAddress).GetIpv4()
                                   << " on port " << m_recvPort << ". Forwarding...");

            if (m_sendSocket == nullptr) {
                NS_LOG_WARN("No send socket available for forwarding!");
                return;
            }

            Ptr<Packet> forwardedPayload = packet->GetPayload();
            int bytesSent = m_sendSocket->SendTo(forwardedPayload, 0, InetSocketAddress(m_targetAddress, m_targetPort));
            if (bytesSent == -1) {
                NS_LOG_ERROR("Failed to send packet from UdpForwarder to " << m_targetAddress << ":" << m_targetPort);
            } else {
                NS_LOG_INFO("UdpForwarder sent " << bytesSent << " bytes to " << m_targetAddress << ":" << m_targetPort);
            }
        } else {
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s UdpForwarder received "
                                   << packet->GetSize() << " bytes from non-IPv4 address.");
        }
    }
}

NS_LOG_COMPONENT_DEFINE("CustomUdpServer");

class CustomUdpServer : public Application {
public:
    static TypeId GetTypeId();
    CustomUdpServer();
    ~CustomUdpServer() override;

    void SetPort(uint16_t port);

protected:
    void DoInitialize() override;
    void DoDispose() override;
    void StartApplication() override;
    void StopApplication() override;

private:
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

TypeId CustomUdpServer::GetTypeId() {
    static TypeId tid = TypeId("ns3::CustomUdpServer")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<CustomUdpServer>()
        .AddAttribute("Port", "Port to listen on.",
                      UintegerValue(10),
                      MakeUintegerAccessor(&CustomUdpServer::m_port),
                      MakeUintegerChecker<uint16_t>());
    return tid;
}

CustomUdpServer::CustomUdpServer() : m_socket(nullptr), m_port(10) {}

CustomUdpServer::~CustomUdpServer() {}

void CustomUdpServer::SetPort(uint16_t port) {
    m_port = port;
}

void CustomUdpServer::DoInitialize() {
    Application::DoInitialize();
}

void CustomUdpServer::DoDispose() {
    if (m_socket != nullptr) {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    Application::DoDispose();
}

void CustomUdpServer::StartApplication() {
    NS_LOG_FUNCTION(this);
    if (m_socket == nullptr) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress localAddress = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(localAddress) == -1) {
            NS_LOG_ERROR("Failed to bind socket to " << localAddress);
            return;
        }
        m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
        NS_LOG_INFO("CustomUdpServer " << GetNode()->GetId() << " listening on " << localAddress);
    }
}

void CustomUdpServer::StopApplication() {
    NS_LOG_FUNCTION(this);
    DoDispose();
}

void CustomUdpServer::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address fromAddress;
    while ((packet = socket->RecvFrom(fromAddress))) {
        if (InetSocketAddress::IsMatchingType(fromAddress)) {
            std::string payload;
            if (packet->GetSize() > 0) {
                uint8_t *buffer = new uint8_t[packet->GetSize() + 1];
                packet->CopyData(buffer, packet->GetSize());
                buffer[packet->GetSize()] = '\0';
                payload = reinterpret_cast<char*>(buffer);
                delete[] buffer;
            } else {
                payload = "(empty)";
            }

            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s CustomUdpServer received "
                                   << packet->GetSize() << " bytes from "
                                   << InetSocketAddress::ConvertFrom(fromAddress).GetIpv4()
                                   << " on port " << m_port << ". Content: '" << payload << "'");
        } else {
            NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s CustomUdpServer received "
                                   << packet->GetSize() << " bytes from non-IPv4 address.");
        }
    }
}

} // namespace ns3

int main (int argc, char *argv[]) {
    ns3::LogComponentEnable("UdpForwarder", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("CustomUdpServer", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);

    ns3::NodeContainer nodes;
    nodes.Create(3); // Node 0: Client, Node 1: Relay, Node 2: Server

    ns3::Ptr<ns3::Node> clientNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> relayNode = nodes.Get(1);
    ns3::Ptr<ns3::Node> serverNode = nodes.Get(2);

    ns3::PointToPointHelper p2pClientRelay;
    p2pClientRelay.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pClientRelay.SetChannelAttribute("Delay", ns3::StringValue("10ms"));
    ns3::NetDeviceContainer clientRelayDevices = p2pClientRelay.Install(clientNode, relayNode);

    ns3::PointToPointHelper p2pRelayServer;
    p2pRelayServer.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pRelayServer.SetChannelAttribute("Delay", ns3::StringValue("10ms"));
    ns3::NetDeviceContainer relayServerDevices = p2pRelayServer.Install(relayNode, serverNode);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper ipClientRelay;
    ipClientRelay.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer clientRelayInterfaces = ipClientRelay.Assign(clientRelayDevices);

    ns3::Ipv4AddressHelper ipRelayServer;
    ipRelayServer.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer relayServerInterfaces = ipRelayServer.Assign(relayServerDevices);

    ns3::Ipv4Address relayClientFacingIp = clientRelayInterfaces.GetAddress(1);
    ns3::Ipv4Address serverIp = relayServerInterfaces.GetAddress(1);

    uint16_t clientToRelayPort = 9;
    uint16_t relayToServerPort = 10;

    ns3::ApplicationContainer serverApps;
    ns3::Ptr<ns3::CustomUdpServer> serverApp = ns3::CreateObject<ns3::CustomUdpServer>();
    serverApp->SetPort(relayToServerPort);
    serverNode->AddApplication(serverApp);
    serverApps.Add(serverApp);
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    ns3::ApplicationContainer relayApps;
    ns3::Ptr<ns3::UdpForwarder> relayApp = ns3::CreateObject<ns3::UdpForwarder>();
    relayApp->SetListeningPort(clientToRelayPort);
    relayApp->SetTarget(serverIp, relayToServerPort);
    relayNode->AddApplication(relayApp);
    relayApps.Add(relayApp);
    relayApps.Start(ns3::Seconds(1.0));
    relayApps.Stop(ns3::Seconds(10.0));

    ns3::UdpClientHelper clientHelper(relayClientFacingIp, clientToRelayPort);
    clientHelper.SetAttribute("MaxPackets", ns3::UintegerValue(1));
    clientHelper.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    
    std::string clientMessage = "Hello from Client!";
    ns3::Ptr<ns3::Packet> clientPacket = ns3::Create<ns3::Packet>((const uint8_t*)clientMessage.c_str(), clientMessage.length() + 1);
    clientHelper.SetAttribute("Packet", ns3::PacketValue(clientPacket));

    ns3::ApplicationContainer clientApps = clientHelper.Install(clientNode);
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}