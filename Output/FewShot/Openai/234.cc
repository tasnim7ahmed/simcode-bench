#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelaySimulation");

class UdpRelayApp : public Application
{
public:
    UdpRelayApp() : m_socket(0), m_rxPort(0), m_txPort(0) {}
    virtual ~UdpRelayApp() { m_socket = 0; }

    void Setup(Address relayListen, uint16_t rxPort, Address forwardAddress, uint16_t txPort)
    {
        m_relayListen = relayListen;
        m_rxPort = rxPort;
        m_forwardAddress = forwardAddress;
        m_txPort = txPort;
    }

private:
    virtual void StartApplication() override
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_relayListen), m_rxPort);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&UdpRelayApp::HandleRead, this));
        }
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            // Forward packet unchanged
            Ptr<Socket> outSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            outSocket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_forwardAddress), m_txPort));
            outSocket->Send(packet->Copy());
            outSocket->Close();
        }
    }

    Ptr<Socket> m_socket;
    Address m_relayListen;
    uint16_t m_rxPort;
    Address m_forwardAddress;
    uint16_t m_txPort;
};

class ServerApp : public Application
{
public:
    ServerApp() : m_socket(0), m_port(0) {}
    virtual ~ServerApp() { m_socket = 0; }

    void Setup(uint16_t port) { m_port = port; }

private:
    virtual void StartApplication() override
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&ServerApp::HandleRead, this));
        }
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            NS_LOG_INFO("Server received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }
    Ptr<Socket> m_socket;
    uint16_t m_port;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpRelaySimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // 0: client, 1: relay, 2: server

    // Links: client <-> relay, relay <-> server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0r = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer drs = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer iface0r = address.Assign(d0r);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifacers = address.Assign(drs);

    // --- Install Server Application on node 2 ---
    uint16_t serverPort = 9000;
    Ptr<ServerApp> serverApp = CreateObject<ServerApp>();
    serverApp->Setup(serverPort);
    nodes.Get(2)->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(1.0));
    serverApp->SetStopTime(Seconds(10.0));

    // --- Install Relay Application on node 1 ---
    Ptr<UdpRelayApp> relayApp = CreateObject<UdpRelayApp>();
    // Relay listens on iface0r.GetAddress(1), port serverPort; forwards to ifacers.GetAddress(1), port serverPort
    relayApp->Setup(iface0r.GetAddress(1), serverPort, ifacers.GetAddress(1), serverPort);
    nodes.Get(1)->AddApplication(relayApp);
    relayApp->SetStartTime(Seconds(1.0));
    relayApp->SetStopTime(Seconds(10.0));

    // --- Install UDP Client on node 0 ---
    UdpClientHelper client(iface0r.GetAddress(1), serverPort);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}