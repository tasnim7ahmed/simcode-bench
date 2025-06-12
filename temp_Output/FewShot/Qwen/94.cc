#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/ipv6-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpIpv6SocketOptions");

class UdpReceiver : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpReceiver")
            .SetParent<Application>()
            .AddConstructor<UdpReceiver>();
        return tid;
    }

    UdpReceiver() {}
    ~UdpReceiver() {}

private:
    virtual void StartApplication() {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv6Address::GetAny(), 9);
        m_socket->Bind(local);
        m_socket->SetRecvPktInfo(true);
        m_socket->SetRecvTclass(true);
        m_socket->SetRecvHopLimit(true);
        m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::ReceivePacket, this));
    }

    virtual void StopApplication() {
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void ReceivePacket(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            Ipv6Header ipv6Header;
            packet->RemoveHeader(ipv6Header);

            uint8_t tclass = ipv6Header.GetTrafficClass();
            uint8_t hopLimit = ipv6Header.GetHopLimit();

            NS_LOG_INFO("Received packet size: " << packet->GetSize()
                        << " TCLASS: " << static_cast<uint32_t>(tclass)
                        << " HOPLIMIT: " << static_cast<uint32_t>(hopLimit));
        }
    }

    Ptr<Socket> m_socket;
};

class UdpSender : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpSender")
            .SetParent<Application>()
            .AddConstructor<UdpSender>()
            .AddAttribute("RemoteAddress", "The destination IPv6 address",
                          Ipv6AddressValue(),
                          MakeIpv6AddressAccessor(&UdpSender::m_peerAddress),
                          MakeIpv6AddressChecker())
            .AddAttribute("RemotePort", "The destination port",
                          UintegerValue(9),
                          MakeUintegerAccessor(&UdpSender::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketSize", "Size of packets to send in bytes.",
                          UintegerValue(512),
                          MakeUintegerAccessor(&UdpSender::m_size),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("NumPackets", "Number of packets to send.",
                          UintegerValue(5),
                          MakeUintegerAccessor(&UdpSender::m_count),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval", "Time between sending packets.",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpSender::m_interval),
                          MakeTimeChecker())
            .AddAttribute("Tclass", "IPv6 Traffic Class value",
                          UintegerValue(0),
                          MakeUintegerAccessor(&UdpSender::m_tclass),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("HopLimit", "IPv6 Hop Limit value",
                          UintegerValue(64),
                          MakeUintegerAccessor(&UdpSender::m_hopLimit),
                          MakeUintegerChecker<uint8_t>());
        return tid;
    }

    UdpSender() : m_sent(0), m_socket(nullptr) {}
    ~UdpSender() {}

private:
    virtual void StartApplication() {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->SetIpv6Tclass(m_tclass);
        m_socket->SetIpv6HopLimit(m_hopLimit);

        m_sendEvent = Simulator::ScheduleNow(&UdpSender::SendPacket, this);
    }

    virtual void StopApplication() {
        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void SendPacket() {
        if (m_sent < m_count) {
            Ptr<Packet> packet = Create<Packet>(m_size);
            m_socket->SendTo(packet, 0, InetSocketAddress(m_peerAddress, m_peerPort));
            NS_LOG_INFO("Sent packet " << m_sent + 1 << " with TCLASS=" << static_cast<uint32_t>(m_tclass)
                        << " and HOPLIMIT=" << static_cast<uint32_t>(m_hopLimit));
            m_sent++;
            m_sendEvent = Simulator::Schedule(m_interval, &UdpSender::SendPacket, this);
        }
    }

    uint32_t m_size;
    uint32_t m_count;
    uint32_t m_sent;
    uint16_t m_peerPort;
    Ipv6Address m_peerAddress;
    uint8_t m_tclass;
    uint8_t m_hopLimit;
    Time m_interval;
    EventId m_sendEvent;
    Ptr<Socket> m_socket;
};

int main(int argc, char *argv[]) {
    uint32_t packetSize = 512;
    uint32_t numPackets = 5;
    double interval = 1.0;
    uint8_t tclass = 0x10;
    uint8_t hopLimit = 64;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of the packets", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("tclass", "IPv6 Traffic Class value", tclass);
    cmd.AddValue("hopLimit", "IPv6 Hop Limit value", hopLimit);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpReceiver", LOG_LEVEL_INFO);
    LogComponentEnable("UdpSender", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv6AddressHelper address;
    Ipv6InterfaceContainer interfaces = address.Assign(devices);

    // Install applications
    UdpReceiverHelper receiver;
    ApplicationContainer serverApp = receiver.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpSenderHelper sender(interfaces.GetAddress(1), 9);
    sender.SetAttribute("PacketSize", UintegerValue(packetSize));
    sender.SetAttribute("NumPackets", UintegerValue(numPackets));
    sender.SetAttribute("Interval", TimeValue(Seconds(interval)));
    sender.SetAttribute("Tclass", UintegerValue(tclass));
    sender.SetAttribute("HopLimit", UintegerValue(hopLimit));

    ApplicationContainer clientApp = sender.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}