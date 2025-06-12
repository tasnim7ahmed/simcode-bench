#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpExample");

class CustomUdpServer : public Application {
public:
    CustomUdpServer();
    virtual ~CustomUdpServer();

    void Setup(uint16_t port);

private:
    virtual void StartApplication();
    virtual void StopApplication();

    void HandleRead(Ptr<Socket> socket);
    void OnDataReceived(Ptr<Socket> socket);

    uint16_t m_port;
    Ptr<Socket> m_socket;
    Address m_localAddress;
    bool m_running;
};

CustomUdpServer::CustomUdpServer() : m_port(0), m_socket(nullptr), m_running(false) {}

CustomUdpServer::~CustomUdpServer() {
    if (m_socket != nullptr) {
        m_socket->Close();
    }
}

void CustomUdpServer::Setup(uint16_t port) {
    m_port = port;
}

void CustomUdpServer::StartApplication() {
    m_running = true;
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
}

void CustomUdpServer::StopApplication() {
    m_running = false;
    if (m_socket != nullptr) {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void CustomUdpServer::HandleRead(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    if (packet->GetSize() > 0) {
        OnDataReceived(socket);
    }
}

void CustomUdpServer::OnDataReceived(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    if (packet->GetSize() > 0) {
        std::cout << "Server received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << ":" << InetSocketAddress::ConvertFrom(from).GetPort() << std::endl;
    }
}

class CustomUdpClient : public Application {
public:
    CustomUdpClient();
    virtual ~CustomUdpClient();

    void Setup(Ipv4Address address1, uint16_t port1, Ipv4Address address2, uint16_t port2, uint32_t packetSize, uint32_t maxPackets, Time interPacketInterval);

private:
    virtual void StartApplication();
    virtual void StopApplication();

    void SendPacket(Ptr<Socket> socket, Ipv4Address address, uint16_t port);
    void ScheduleTx();

    Ipv4Address m_remoteAddress1;
    uint16_t m_remotePort1;
    Ipv4Address m_remoteAddress2;
    uint16_t m_remotePort2;
    uint32_t m_packetSize;
    uint32_t m_maxPackets;
    Time m_interPacketInterval;
    Ptr<Socket> m_socket1;
    Ptr<Socket> m_socket2;
    EventId m_sendEvent;
    uint32_t m_packetsSent;
    bool m_running;
};

CustomUdpClient::CustomUdpClient() : m_remotePort1(0), m_packetSize(0), m_maxPackets(0), m_interPacketInterval(Seconds(0)), m_socket1(nullptr), m_packetsSent(0), m_running(false) {}

CustomUdpClient::~CustomUdpClient() {
    if (m_socket1 != nullptr) {
        m_socket1->Close();
    }
    if (m_socket2 != nullptr) {
        m_socket2->Close();
    }
}

void CustomUdpClient::Setup(Ipv4Address address1, uint16_t port1, Ipv4Address address2, uint16_t port2, uint32_t packetSize, uint32_t maxPackets, Time interPacketInterval) {
    m_remoteAddress1 = address1;
    m_remotePort1 = port1;
    m_remoteAddress2 = address2;
    m_remotePort2 = port2;
    m_packetSize = packetSize;
    m_maxPackets = maxPackets;
    m_interPacketInterval = interPacketInterval;
}

void CustomUdpClient::StartApplication() {
    m_running = true;
    m_packetsSent = 0;

    m_socket1 = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket1->Connect(InetSocketAddress(m_remoteAddress1, m_remotePort1));
    m_socket1->SetAllowBroadcast(true);

    m_socket2 = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket2->Connect(InetSocketAddress(m_remoteAddress2, m_remotePort2));
    m_socket2->SetAllowBroadcast(true);

    ScheduleTx();
}

void CustomUdpClient::StopApplication() {
    m_running = false;
    Simulator::Cancel(m_sendEvent);

    if (m_socket1 != nullptr) {
        m_socket1->Close();
        m_socket1 = nullptr;
    }
    if (m_socket2 != nullptr) {
        m_socket2->Close();
        m_socket2 = nullptr;
    }
}

void CustomUdpClient::SendPacket(Ptr<Socket> socket, Ipv4Address address, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    socket->Send(packet);
    std::cout << "Client sent " << m_packetSize << " bytes to " << address << ":" << port << std::endl;
    m_packetsSent++;
}

void CustomUdpClient::ScheduleTx() {
    if (m_running) {
        Time tNext(m_interPacketInterval);
        m_sendEvent = Simulator::Schedule(tNext, &CustomUdpClient::ScheduleTx, this);
        if (m_packetsSent < m_maxPackets) {
            SendPacket(m_socket1, m_remoteAddress1, m_remotePort1);
            SendPacket(m_socket2, m_remoteAddress2, m_remotePort2);
        } else {
            Simulator::Cancel(m_sendEvent);
        }
    }
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer clientNode;
    clientNode.Create(1);
    NodeContainer serverNodes;
    serverNodes.Create(2);
    NodeContainer switchNode;
    switchNode.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer clientDevices = csma.Install(NodeContainer(clientNode.Get(0), switchNode.Get(0)));
    NetDeviceContainer server1Devices = csma.Install(NodeContainer(serverNodes.Get(0), switchNode.Get(0)));
    NetDeviceContainer server2Devices = csma.Install(NodeContainer(serverNodes.Get(1), switchNode.Get(0)));

    InternetStackHelper internet;
    internet.Install(clientNode);
    internet.Install(serverNodes);
    internet.Install(switchNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);
    address.NewNetwork();
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer server1Interfaces = address.Assign(server1Devices);
    address.NewNetwork();
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer server2Interfaces = address.Assign(server2Devices);

    uint16_t port1 = 9000;
    uint16_t port2 = 9001;

    CustomUdpServer server1App;
    server1App.Setup(port1);
    ApplicationContainer server1Apps;
    server1Apps.Add(server1App.Install(serverNodes.Get(0)));
    server1Apps.Start(Seconds(1.0));
    server1Apps.Stop(Seconds(10.0));

    CustomUdpServer server2App;
    server2App.Setup(port2);
    ApplicationContainer server2Apps;
    server2Apps.Add(server2App.Install(serverNodes.Get(1)));
    server2Apps.Start(Seconds(1.0));
    server2Apps.Stop(Seconds(10.0));

    CustomUdpClient clientApp;
    clientApp.Setup(server1Interfaces.GetAddress(0), port1, server2Interfaces.GetAddress(0), port2, 1024, 5, Seconds(1));
    ApplicationContainer clientApps;
    clientApps.Add(clientApp.Install(clientNode.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}