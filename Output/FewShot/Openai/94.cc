#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6UdpTclassHoplimitExample");

class UdpIpv6Receiver : public Application
{
public:
    UdpIpv6Receiver()
      : m_socket(0),
        m_recvTclass(false),
        m_recvHoplimit(false)
    {}
    virtual ~UdpIpv6Receiver() {
        m_socket = 0;
    }

    void Setup(Address listenAddress, uint16_t port, bool recvTclass, bool recvHoplimit)
    {
        m_listenAddress = listenAddress;
        m_port = port;
        m_recvTclass = recvTclass;
        m_recvHoplimit = recvHoplimit;
    }

private:
    virtual void StartApplication(void)
    {
        if (!m_socket) {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            Inet6SocketAddress bindAddr = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);
            m_socket->Bind(bindAddr);
            m_socket->SetRecvCallback(MakeCallback(&UdpIpv6Receiver::HandleRead, this));

            int fd = -1;
            m_socket->GetSockName(fd);
            if (m_recvTclass) {
                int opt = 1;
                m_socket->SetSockOpt6(SOL_IPV6, IPV6_RECVTCLASS, &opt, sizeof(opt));
            }
            if (m_recvHoplimit) {
                int opt = 1;
                m_socket->SetSockOpt6(SOL_IPV6, IPV6_RECVHOPLIMIT, &opt, sizeof(opt));
            }
        }
    }

    virtual void StopApplication(void)
    {
        if (m_socket) {
            m_socket->Close();
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Address from;
        uint8_t data[65536];
        struct msghdr msg;
        struct iovec iov;
        uint8_t ctrl[512];
        size_t tclass = 0;
        size_t hoplimit = 0;

        while (socket->GetRxAvailable() > 0) {
            Ptr<Packet> packet;
            struct sockaddr_storage srcAddr;
            socklen_t addrLen = sizeof(srcAddr);

            iov.iov_base = data;
            iov.iov_len = sizeof(data);

            memset(&msg, 0, sizeof(msg));
            msg.msg_name = &srcAddr;
            msg.msg_namelen = addrLen;
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_control = ctrl;
            msg.msg_controllen = sizeof(ctrl);

            ssize_t n = socket->RecvMsg(&msg, Socket::MSG_DONTWAIT);
            if (n <= 0) {
                break;
            }

            tclass = 0;
            hoplimit = 0;

            for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg,cmsg)) {
                if (m_recvTclass && cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_TCLASS) {
                    tclass = *((uint8_t *)CMSG_DATA(cmsg));
                }
                if (m_recvHoplimit && cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_HOPLIMIT) {
                    hoplimit = *((int *)CMSG_DATA(cmsg));
                }
            }

            packet = Create<Packet>(reinterpret_cast<const uint8_t*>(data), n);

            socket->GetSockName(from);

            NS_LOG_INFO("Received " << n << " bytes from "
                        << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
                        << ", TCLASS=" << tclass
                        << ", HOPLIMIT=" << hoplimit);

            std::cout << "At time " << Simulator::Now().GetSeconds()
                      << "s node " << GetNode()->GetId()
                      << " received " << n << " bytes, TCLASS=" << tclass
                      << ", HOPLIMIT=" << hoplimit << std::endl;
        }
    }

    Ptr<Socket> m_socket;
    Address m_listenAddress;
    uint16_t m_port;
    bool m_recvTclass;
    bool m_recvHoplimit;
};

class UdpIpv6Sender : public Application
{
public:
    UdpIpv6Sender()
      : m_socket(0),
        m_sendAddress(),
        m_port(0),
        m_packetSize(512),
        m_nPackets(1),
        m_interval(Seconds(1.0)),
        m_sent(0),
        m_tclass(0),
        m_hoplimit(0),
        m_setTclass(false),
        m_setHoplimit(false)
    {}

    void Setup(Address sendAddress, uint16_t port, uint32_t packetSize, uint32_t nPackets,
               Time interval, int tclass, int hoplimit, bool setTclass, bool setHoplimit)
    {
        m_sendAddress = sendAddress;
        m_port = port;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_interval = interval;
        m_tclass = tclass;
        m_hoplimit = hoplimit;
        m_setTclass = setTclass;
        m_setHoplimit = setHoplimit;
    }

private:
    virtual void StartApplication(void)
    {
        if (!m_socket) {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        }
        if (m_setTclass) {
            m_socket->SetIpTclass(m_tclass);
        }
        if (m_setHoplimit) {
            m_socket->SetIpHopLimit(m_hoplimit);
        }
        m_sent = 0;
        SendPacket();
    }

    virtual void StopApplication(void)
    {
        if (m_socket) {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->SendTo(packet, 0, m_sendAddress);
        ++m_sent;
        NS_LOG_INFO("Sender sent packet " << m_sent);
        if (m_sent < m_nPackets) {
            Simulator::Schedule(m_interval, &UdpIpv6Sender::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_sendAddress;
    uint16_t m_port;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_sent;
    int m_tclass;
    int m_hoplimit;
    bool m_setTclass;
    bool m_setHoplimit;
};

int main(int argc, char *argv[])
{
    uint32_t packetSize = 512;
    uint32_t nPackets = 5;
    double interval = 1.0;
    int tclass = 0;
    int hoplimit = 0;
    bool setTclass = false;
    bool setHoplimit = false;
    bool recvTclass = false;
    bool recvHoplimit = false;

    CommandLine cmd;
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("nPackets", "Number of packets to send", nPackets);
    cmd.AddValue("interval", "Interval in seconds between packets", interval);
    cmd.AddValue("tclass", "IPv6 TCLASS value to set (0-255)", tclass);
    cmd.AddValue("hoplimit", "IPv6 HOPLIMIT value to set (0-255)", hoplimit);
    cmd.AddValue("setTclass", "Set IPv6 TCLASS on sender (bool)", setTclass);
    cmd.AddValue("setHoplimit", "Set IPv6 HOPLIMIT on sender (bool)", setHoplimit);
    cmd.AddValue("recvTclass", "Receiver enables RECVTCLASS socket option", recvTclass);
    cmd.AddValue("recvHoplimit", "Receiver enables RECVHOPLIMIT socket option", recvHoplimit);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("6560ns"));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaces = ipv6.Assign(devices);
    ifaces.SetForwarding(0, true);
    ifaces.SetDefaultRouteInAllNodes(0);

    uint16_t port = 8888;
    Ptr<UdpIpv6Receiver> receiver = CreateObject<UdpIpv6Receiver>();
    receiver->Setup(Inet6SocketAddress(ifaces.GetAddress(1, 1), port), port, recvTclass, recvHoplimit);
    nodes.Get(1)->AddApplication(receiver);
    receiver->SetStartTime(Seconds(0.1));
    receiver->SetStopTime(Seconds(20.0));

    Ptr<UdpIpv6Sender> sender = CreateObject<UdpIpv6Sender>();
    sender->Setup(Inet6SocketAddress(ifaces.GetAddress(1, 1), port), port, packetSize, nPackets, Seconds(interval),
                  tclass, hoplimit, setTclass, setHoplimit);
    nodes.Get(0)->AddApplication(sender);
    sender->SetStartTime(Seconds(1.0));
    sender->SetStopTime(Seconds(20.0));

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}