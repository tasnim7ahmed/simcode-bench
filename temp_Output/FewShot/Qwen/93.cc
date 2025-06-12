#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpSocketOptionsExample");

class UdpReceiver : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("UdpReceiver")
            .SetParent<Application>()
            .AddConstructor<UdpReceiver>();
        return tid;
    }

    UdpReceiver() {}
    virtual ~UdpReceiver() {}

private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
    }

    virtual void StopApplication(void) {
        if (m_socket)
            m_socket->Close();
    }

    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            Ipv4Header ipHeader;
            packet->PeekHeader(ipHeader);
            NS_LOG_INFO("Received packet with size " << packet->GetSize()
                      << ", TOS: " << (uint32_t)ipHeader.GetTos()
                      << ", TTL: " << (uint32_t)ipHeader.GetTtl());
        }
    }

    Ptr<Socket> m_socket;
};

class UdpSender : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("UdpSender")
            .SetParent<Application>()
            .AddConstructor<UdpSender>()
            .AddAttribute("Remote", "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&UdpSender::m_peer),
                          MakeAddressChecker())
            .AddAttribute("PacketSize", "Size of packets to send",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpSender::m_size),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("NumberofPackets", "Number of packets to send",
                          UintegerValue(5),
                          MakeUintegerAccessor(&UdpSender::m_count),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval", "Time between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpSender::m_interval),
                          MakeTimeChecker())
            .AddAttribute("IP_TTL", "TTL value for IP header",
                          UintegerValue(64),
                          MakeUintegerAccessor(&UdpSender::m_ttl),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("IP_TOS", "TOS value for IP header",
                          UintegerValue(0),
                          MakeUintegerAccessor(&UdpSender::m_tos),
                          MakeUintegerChecker<uint8_t>());
        return tid;
    }

    UdpSender() : m_sent(0) {}
    virtual ~UdpSender() {}

private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->SetAllowBroadcast(true);
        m_socket->SetIpTtl(m_ttl);
        m_socket->SetIpTos(m_tos);
        m_socket->Connect(m_peer);

        Simulator::ScheduleNow(&UdpSender::SendPacket, this);
    }

    virtual void StopApplication(void) {
        if (m_socket)
            m_socket->Close();
    }

    void SendPacket(void) {
        if (m_sent < m_count) {
            Ptr<Packet> packet = Create<Packet>(m_size);
            m_socket->Send(packet);
            NS_LOG_INFO("Sent packet " << m_sent + 1 << " with size " << m_size
                      << ", TOS: " << (uint32_t)m_tos
                      << ", TTL: " << (uint32_t)m_ttl);
            m_sent++;
            Simulator::Schedule(m_interval, &UdpSender::SendPacket, this);
        }
    }

    uint32_t m_count;
    uint32_t m_size;
    uint32_t m_sent;
    Time m_interval;
    uint8_t m_ttl;
    uint8_t m_tos;
    Address m_peer;
    Ptr<Socket> m_socket;
};

int main(int argc, char *argv[]) {
    uint32_t packetSize = 1024;
    uint32_t count = 5;
    double interval = 1.0;
    uint8_t ipTtl = 64;
    uint8_t ipTos = 0;
    bool enableRecvTos = false;
    bool enableRecvTtl = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("PacketSize", "Size of packets to send", packetSize);
    cmd.AddValue("NumberofPackets", "Number of packets to send", count);
    cmd.AddValue("Interval", "Time between packets (seconds)", interval);
    cmd.AddValue("IP_TTL", "TTL value for IP header", ipTtl);
    cmd.AddValue("IP_TOS", "TOS value for IP header", ipTos);
    cmd.AddValue("RECVTOS", "Enable receiving TOS information", enableRecvTos);
    cmd.AddValue("RECVTTL", "Enable receiving TTL information", enableRecvTtl);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpSocketOptionsExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup receiver application
    UdpReceiverHelper receiver;
    ApplicationContainer serverApp = receiver.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup sender application
    UdpSenderHelper sender(interfaces.GetAddress(1), 9);
    sender.SetAttribute("PacketSize", UintegerValue(packetSize));
    sender.SetAttribute("NumberofPackets", UintegerValue(count));
    sender.SetAttribute("Interval", TimeValue(Seconds(interval)));
    sender.SetAttribute("IP_TTL", UintegerValue(ipTtl));
    sender.SetAttribute("IP_TOS", UintegerValue(ipTos));

    ApplicationContainer clientApp = sender.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set socket options on receiver side if enabled
    if (enableRecvTos || enableRecvTtl) {
        Config::Set("/NodeList/1/ApplicationList/*/$ns3::UdpReceiver/m_socket/IP_RECVTOS", BooleanValue(enableRecvTos));
        Config::Set("/NodeList/1/ApplicationList/*/$ns3::UdpReceiver/m_socket/IP_RECVTTL", BooleanValue(enableRecvTtl));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}