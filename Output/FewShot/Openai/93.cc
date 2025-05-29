#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTosTtlExample");

class UdpReceiver : public Application
{
public:
    UdpReceiver();
    virtual ~UdpReceiver();

    void Setup(Address address, uint16_t port, bool recvTos, bool recvTtl);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket>     m_socket;
    Address         m_local;
    uint16_t        m_port;
    bool            m_running;
    bool            m_recvTos;
    bool            m_recvTtl;
};

UdpReceiver::UdpReceiver()
    : m_socket(0),
      m_port(0),
      m_running(false),
      m_recvTos(false),
      m_recvTtl(false)
{
}

UdpReceiver::~UdpReceiver()
{
    m_socket = 0;
}

void UdpReceiver::Setup(Address address, uint16_t port, bool recvTos, bool recvTtl)
{
    m_local = address;
    m_port = port;
    m_recvTos = recvTos;
    m_recvTtl = recvTtl;
}

void UdpReceiver::StartApplication(void)
{
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_local), m_port);
        m_socket->Bind(local);
        if (m_recvTos)
        {
            int opt = 1;
#ifdef IP_RECVTOS
            m_socket->SetSockOpt(SOL_IP, IP_RECVTOS, &opt, sizeof(opt));
#endif
        }
        if (m_recvTtl)
        {
            int opt = 1;
#ifdef IP_RECVTTL
            m_socket->SetSockOpt(SOL_IP, IP_RECVTTL, &opt, sizeof(opt));
#endif
        }
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
    m_running = true;
}

void UdpReceiver::StopApplication(void)
{
    m_running = false;
    if (m_socket)
        m_socket->Close();
}

void UdpReceiver::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while(Ptr<Packet> packet = socket->RecvFrom(from))
    {
        SocketIpTosTag tosTag;
        SocketIpTtlTag ttlTag;
        uint8_t tos = 0;
        uint8_t ttl = 0;

        if (packet->PeekPacketTag(tosTag)) {
            tos = tosTag.GetTos();
        }
        if (packet->PeekPacketTag(ttlTag)) {
            ttl = ttlTag.GetTtl();
        }
        NS_LOG_UNCOND("Received packet of size " << packet->GetSize() <<
                      " from " << InetSocketAddress::ConvertFrom(from).GetIpv4() <<
                      " TOS=" << uint32_t(tos) <<
                      " TTL=" << uint32_t(ttl) <<
                      " time=" << Simulator::Now().GetSeconds());
    }
}

class UdpSender : public Application
{
public:
    UdpSender();
    virtual ~UdpSender();

    void Setup(Address destAddress, uint16_t port, uint32_t pktSize, uint32_t nPkts, Time interval, int tos, int ttl);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);

    Ptr<Socket>     m_socket;
    Address         m_peer;
    uint16_t        m_port;
    bool            m_running;
    uint32_t        m_pktSize;
    uint32_t        m_nPkts;
    uint32_t        m_sent;
    EventId         m_sendEvent;
    Time            m_interval;
    int             m_tos;
    int             m_ttl;
};

UdpSender::UdpSender()
    : m_socket(0),
      m_port(0),
      m_running(false),
      m_pktSize(1024),
      m_nPkts(1),
      m_sent(0),
      m_interval(Seconds(1.0)),
      m_tos(-1),
      m_ttl(-1)
{
}

UdpSender::~UdpSender()
{
    m_socket = 0;
}

void UdpSender::Setup(Address destAddress, uint16_t port, uint32_t pktSize, uint32_t nPkts, Time interval, int tos, int ttl)
{
    m_peer = destAddress;
    m_port = port;
    m_pktSize = pktSize;
    m_nPkts = nPkts;
    m_interval = interval;
    m_tos = tos;
    m_ttl = ttl;
}

void UdpSender::StartApplication(void)
{
    m_running = true;
    m_sent = 0;
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            m_socket->Connect(m_peer);
        }
        if (m_tos >= 0)
        {
            int tos = m_tos;
#ifdef IP_TOS
            m_socket->SetSockOpt(SOL_IP, IP_TOS, &tos, sizeof(tos));
#endif
        }
        if (m_ttl >= 0)
        {
            int ttl = m_ttl;
#ifdef IP_TTL
            m_socket->SetSockOpt(SOL_IP, IP_TTL, &ttl, sizeof(ttl));
#endif
        }
    }
    SendPacket();
}

void UdpSender::StopApplication(void)
{
    m_running = false;
    if (m_sendEvent.IsRunning())
        Simulator::Cancel(m_sendEvent);
    if (m_socket)
        m_socket->Close();
}

void UdpSender::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    m_socket->Send(packet);
    ++m_sent;
    if (m_sent < m_nPkts)
    {
        ScheduleTx();
    }
}

void UdpSender::ScheduleTx(void)
{
    if (m_running)
    {
        m_sendEvent = Simulator::Schedule(m_interval, &UdpSender::SendPacket, this);
    }
}

int main(int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0;
    int tos = -1;
    int ttl = -1;
    bool recvTos = false;
    bool recvTtl = false;

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of each UDP packet (bytes)", packetSize);
    cmd.AddValue("numPackets", "Number of UDP packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets (seconds)", interval);
    cmd.AddValue("tos", "IP_TOS value for outgoing packets (-1: not set)", tos);
    cmd.AddValue("ttl", "IP_TTL value for outgoing packets (-1: not set)", ttl);
    cmd.AddValue("recvTos", "Enable RECVTOS on receiver", recvTos);
    cmd.AddValue("recvTtl", "Enable RECVTTL on receiver", recvTtl);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 5000;

    Ptr<UdpReceiver> receiverApp = CreateObject<UdpReceiver>();
    receiverApp->Setup(interfaces.GetAddress(1), port, recvTos, recvTtl);
    nodes.Get(1)->AddApplication(receiverApp);
    receiverApp->SetStartTime(Seconds(0.0));
    receiverApp->SetStopTime(Seconds(20.0));

    Ptr<UdpSender> senderApp = CreateObject<UdpSender>();
    senderApp->Setup(InetSocketAddress(interfaces.GetAddress(1), port), port, packetSize, numPackets, Seconds(interval), tos, ttl);
    nodes.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(1.0));
    senderApp->SetStopTime(Seconds(20.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}