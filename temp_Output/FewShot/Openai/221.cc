#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiServerUdpSimulation");

// Custom UDP Server Application
class MyUdpServer : public Application
{
public:
    MyUdpServer() : m_socket(0) {}
    virtual ~MyUdpServer() { m_socket = 0; }

    void Setup(uint16_t port)
    {
        m_port = port;
    }

private:
    virtual void StartApplication()
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&MyUdpServer::HandleRead, this));
        }
    }

    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
            NS_LOG_INFO("Server on port " << m_port <<
                        " received " << packet->GetSize() << " bytes from " <<
                        address.GetIpv4());
        }
    }

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

// Custom UDP Client Application
class MyUdpClient : public Application
{
public:
    MyUdpClient() {}
    virtual ~MyUdpClient() {}

    void Setup(std::vector<Ipv4Address> dstAddrs,
               std::vector<uint16_t> dstPorts,
               uint32_t packetSize,
               Time interval)
    {
        m_dstAddrs = dstAddrs;
        m_dstPorts = dstPorts;
        m_packetSize = packetSize;
        m_interval = interval;
        m_sendEvent = EventId();
    }

private:
    virtual void StartApplication()
    {
        for (size_t i = 0; i < m_dstAddrs.size(); ++i)
        {
            Ptr<Socket> socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_sockets.push_back(socket);
            InetSocketAddress remote = InetSocketAddress(m_dstAddrs[i], m_dstPorts[i]);
            socket->Connect(remote);
        }
        m_packetCount = 0;
        ScheduleSend();
    }

    virtual void StopApplication()
    {
        for (auto &socket : m_sockets)
        {
            if (socket)
            {
                socket->Close();
            }
        }
        m_sockets.clear();
        Simulator::Cancel(m_sendEvent);
    }

    void ScheduleSend()
    {
        m_sendEvent = Simulator::Schedule(m_interval, &MyUdpClient::SendPacket, this);
    }

    void SendPacket()
    {
        for (size_t i = 0; i < m_sockets.size(); ++i)
        {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);
            m_sockets[i]->Send(packet);
            NS_LOG_INFO("Client sent " << m_packetSize
                        << " bytes to " << m_dstAddrs[i]
                        << " port " << m_dstPorts[i]);
        }
        m_packetCount++;
        ScheduleSend();
    }

    std::vector<Ipv4Address> m_dstAddrs;
    std::vector<uint16_t> m_dstPorts;
    uint32_t m_packetSize;
    Time m_interval;
    std::vector<Ptr<Socket>> m_sockets;
    EventId m_sendEvent;
    uint32_t m_packetCount;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("MultiServerUdpSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // node 0: client, node 1: server1, node 2: server2

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server 1
    uint16_t port1 = 8080;
    Ptr<MyUdpServer> server1App = CreateObject<MyUdpServer>();
    server1App->Setup(port1);
    nodes.Get(1)->AddApplication(server1App);
    server1App->SetStartTime(Seconds(0.0));
    server1App->SetStopTime(Seconds(10.0));

    // Server 2
    uint16_t port2 = 9090;
    Ptr<MyUdpServer> server2App = CreateObject<MyUdpServer>();
    server2App->Setup(port2);
    nodes.Get(2)->AddApplication(server2App);
    server2App->SetStartTime(Seconds(0.0));
    server2App->SetStopTime(Seconds(10.0));

    // Client
    std::vector<Ipv4Address> dstAddrs;
    std::vector<uint16_t> dstPorts;
    dstAddrs.push_back(interfaces.GetAddress(1));
    dstAddrs.push_back(interfaces.GetAddress(2));
    dstPorts.push_back(port1);
    dstPorts.push_back(port2);

    Ptr<MyUdpClient> clientApp = CreateObject<MyUdpClient>();
    clientApp->Setup(dstAddrs, dstPorts, 512, Seconds(1.0));
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}