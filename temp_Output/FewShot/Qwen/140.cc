#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

class TcpServer : public Application {
public:
    TcpServer();
    virtual ~TcpServer();

    static TypeId GetTypeId(void);
    void StartApplication(void);
    void StopApplication(void);

private:
    void HandleAccept(Ptr<Socket> s, const Address& from);
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    Address m_local;
};

TcpServer::TcpServer() : m_socket(0) {}

TcpServer::~TcpServer() {
    m_socket = 0;
}

TypeId TcpServer::GetTypeId(void) {
    static TypeId tid = TypeId("TcpServer")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial");
    return tid;
}

void TcpServer::StartApplication(void) {
    if (!m_socket) {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        m_socket->Bind(m_local);
        m_socket->Listen();
        m_socket->SetAcceptCallback(MakeCallback(&TcpServer::HandleAccept, this), nullptr);
    }
}

void TcpServer::StopApplication(void) {
    if (m_socket) {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void TcpServer::HandleAccept(Ptr<Socket> s, const Address& from) {
    NS_LOG_INFO("TCP connection accepted from " << from);
    s->SetRecvCallback(MakeCallback(&TcpServer::HandleRead, this));
}

void TcpServer::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {
            char *buffer = new char[packet->GetSize()];
            packet->CopyData(buffer, packet->GetSize());
            NS_LOG_INFO("Received data on server: \"" << buffer << "\" from " << from);
            delete[] buffer;
        }
    }
}

class TcpClient : public Application {
public:
    TcpClient();
    virtual ~TcpClient();

    static TypeId GetTypeId(void);
    void SetRemote(Ipv4Address ip, uint16_t port);
    void StartApplication(void);
    void StopApplication(void);

private:
    void SendPacket(void);
    void HandleConnect(Ptr<Socket>);
    void HandleSend(Ptr<Socket>, uint32_t);

    Ipv4Address m_serverIp;
    uint16_t m_serverPort;
    Ptr<Socket> m_socket;
    EventId m_sendEvent;
    bool m_connected;
};

TcpClient::TcpClient()
    : m_serverPort(0),
      m_socket(0),
      m_connected(false) {}

TcpClient::~TcpClient() {
    m_socket = 0;
}

TypeId TcpClient::GetTypeId(void) {
    static TypeId tid = TypeId("TcpClient")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial");
    return tid;
}

void TcpClient::SetRemote(Ipv4Address ip, uint16_t port) {
    m_serverIp = ip;
    m_serverPort = port;
}

void TcpClient::StartApplication(void) {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->SetConnectCallback(MakeCallback(&TcpClient::HandleConnect, this), MakeNullCallback<void, Ptr<Socket>>());
    m_socket->SetAllowBroadcast(true);
    InetSocketAddress remote = InetSocketAddress(m_serverIp, m_serverPort);
    m_socket->Connect(remote);
}

void TcpClient::StopApplication(void) {
    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket) {
        m_socket->Close();
    }
}

void TcpClient::HandleConnect(Ptr<Socket> socket) {
    NS_LOG_INFO("Connected to TCP server");
    m_connected = true;
    SendPacket();
}

void TcpClient::SendPacket(void) {
    std::string message = "Hello from TCP client!";
    Ptr<Packet> packet = Create<Packet>((const uint8_t*)message.c_str(), message.size());
    m_socket->Send(packet);
    NS_LOG_INFO("Sent TCP packet with size: " << message.size());

    m_sendEvent = Simulator::Schedule(Seconds(1.0), &TcpClient::SendPacket, this);
}

int main(int argc, char *argv[]) {
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

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

    // Install server application
    TcpServer serverApp;
    serverApp.SetLocal(InetSocketAddress(Ipv4Address::GetAny(), 5000));
    ApplicationContainer serverContainer;
    serverContainer.Add(&serverApp);
    serverContainer.Start(Seconds(1.0));
    serverContainer.Stop(Seconds(10.0));

    // Install client application
    TcpClient clientApp;
    clientApp.SetRemote(interfaces.GetAddress(1), 5000);
    ApplicationContainer clientContainer;
    clientContainer.Add(&clientApp);
    clientContainer.Start(Seconds(2.0));
    clientContainer.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}