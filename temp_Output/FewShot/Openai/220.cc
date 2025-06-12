#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RawUdpSocketExample");

class UdpServerApp : public Application
{
public:
    UdpServerApp() : m_socket(0), m_port(0) {}

    void Setup(uint16_t port)
    {
        m_port = port;
    }

private:
    virtual void StartApplication(void)
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
        }
    }
    virtual void StopApplication(void)
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Address from;
        Ptr<Packet> packet;
        while ((packet = socket->RecvFrom(from)))
        {
            NS_LOG_INFO("Server received packet, size: " << packet->GetSize() 
                        << " bytes from " 
                        << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

class UdpClientApp : public Application
{
public:
    UdpClientApp() : m_socket(0), m_peerPort(0), m_sendEvent(), m_running(false), m_pktSize(1024), m_interval(Seconds(1.0)), m_nSent(0) {}

    void Setup(Address peerAddress, uint16_t port, uint32_t packetSize, Time interval)
    {
        m_peerAddress = peerAddress;
        m_peerPort = port;
        m_pktSize = packetSize;
        m_interval = interval;
    }

private:
    virtual void StartApplication(void)
    {
        m_running = true;
        m_nSent = 0;
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));
        SendPacket();
    }
    virtual void StopApplication(void)
    {
        m_running = false;
        if (m_sendEvent.IsRunning())
            Simulator::Cancel(m_sendEvent);
        if (m_socket)
            m_socket->Close();
    }

    void SendPacket(void)
    {
        if (!m_running) return;
        Ptr<Packet> packet = Create<Packet>(m_pktSize);
        m_socket->Send(packet);
        ++m_nSent;
        NS_LOG_INFO("Client sent packet #" << m_nSent);
        m_sendEvent = Simulator::Schedule(m_interval, &UdpClientApp::SendPacket, this);
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_pktSize;
    Time m_interval;
    uint32_t m_nSent;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("RawUdpSocketExample", LOG_LEVEL_INFO);

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

    // Server app (node 1)
    uint16_t port = 9000;
    Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
    serverApp->Setup(port);
    nodes.Get(1)->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(1.0));
    serverApp->SetStopTime(Seconds(10.0));

    // Client app (node 0)
    Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
    clientApp->Setup(interfaces.GetAddress(1), port, 1024, Seconds(1.0));
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(2.0));
    clientApp->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}