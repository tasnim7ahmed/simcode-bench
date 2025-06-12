#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

class TcpServer : public Application {
public:
    TcpServer() : m_socket(nullptr) {}
    virtual ~TcpServer();

    static TypeId GetTypeId(void);
    void StartApplication(void);
    void StopApplication(void);

private:
    void HandleAccept(Ptr<Socket> s, const Address& from);
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
};

TcpServer::~TcpServer() {
    if (m_socket) {
        m_socket->Close();
        m_socket = nullptr;
    }
}

TypeId TcpServer::GetTypeId(void) {
    static TypeId tid = TypeId("TcpServer")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<TcpServer>();
    return tid;
}

void TcpServer::StartApplication(void) {
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
        m_socket->Bind(local);
        m_socket->Listen();
        m_socket->SetAcceptCallback(MakeCallback(&TcpServer::HandleAccept, this));
    }
}

void TcpServer::StopApplication(void) {
    if (m_socket) {
        m_socket->Close();
        m_socket = nullptr;
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
        NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
    }
}

class TcpClient : public Application {
public:
    TcpClient();
    virtual ~TcpClient();

    static TypeId GetTypeId(void);
    void StartApplication(void);
    void StopApplication(void);

private:
    void ConnectSucceeded(Ptr<Socket>);
    void ConnectFailed(Ptr<Socket>, const char*);
    void SendData();

    Ptr<Socket> m_socket;
    Address m_peer;
    EventId m_sendEvent;
};

TcpClient::TcpClient() : m_socket(nullptr), m_sendEvent() {}

TcpClient::~TcpClient() {
    if (m_socket) {
        m_socket->Close();
        m_socket = nullptr;
    }
    CancelEvents();
}

TypeId TcpClient::GetTypeId(void) {
    static TypeId tid = TypeId("TcpClient")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<TcpClient>();
    return tid;
}

void TcpClient::StartApplication(void) {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->SetConnectCallback(
        MakeCallback(&TcpClient::ConnectSucceeded, this),
        MakeCallback(&TcpClient::ConnectFailed, this));

    m_socket->Connect(m_peer);
}

void TcpClient::StopApplication(void) {
    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket) {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void TcpClient::ConnectSucceeded(Ptr<Socket> socket) {
    NS_LOG_INFO("TCP connection succeeded.");
    m_sendEvent = Simulator::Schedule(Seconds(1.0), &TcpClient::SendData, this);
}

void TcpClient::ConnectFailed(Ptr<Socket>, const char* reason) {
    NS_LOG_ERROR("TCP connection failed: " << reason);
}

void TcpClient::SendData() {
    if (m_socket) {
        std::string data = "Hello from TCP client!";
        Ptr<Packet> packet = Create<Packet>((const uint8_t*)data.c_str(), data.size());
        NS_LOG_INFO("Sending TCP packet with size " << packet->GetSize());
        m_socket->Send(packet);
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install server on node 1
    ApplicationContainer serverApp;
    serverApp.Add(CreateObject<TcpServer>());
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(5.0));

    // Install client on node 0
    Ptr<TcpClient> client = CreateObject<TcpClient>();
    client->SetStartTime(Seconds(2.0));
    client->SetStopTime(Seconds(5.0));
    client->m_peer = InetSocketAddress(interfaces.GetAddress(1), 8080);
    nodes.Get(0)->AddApplication(client);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}