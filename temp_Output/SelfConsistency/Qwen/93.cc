#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTosTtlExample");

class UdpServerWithMetadata : public UdpServer
{
public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("UdpServerWithMetadata")
                            .SetParent<UdpServer>()
                            .AddConstructor<UdpServerWithMetadata>();
        return tid;
    }

    void HandleRead(Ptr<Socket> socket) override
    {
        NS_LOG_FUNCTION(this << socket);
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            if (InetSocketAddress::IsMatchingType(from))
            {
                InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
                Ipv4Header ipHeader;
                SocketIpTtlTag ttlTag;
                SocketIpv4TosTag tosTag;
                bool hasTtl = packet->RemovePacketTag(ttlTag);
                bool hasTos = packet->RemovePacketTag(tosTag);
                packet->PeekHeader(ipHeader);

                uint8_t tosValue = hasTos ? tosTag.GetTos() : 0;
                uint8_t ttlValue = hasTtl ? ttlTag.GetTtl() : ipHeader.GetTtl();

                NS_LOG_UNCOND("Received packet size: " << packet->GetSize()
                                                     << " from " << addr.GetIpv4()
                                                     << " TOS: " << (uint32_t)tosValue
                                                     << " TTL: " << (uint32_t)ttlValue);
            }
        }
    }
};

int main(int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    uint32_t numPackets = 5;
    double interval = 1.0;
    uint8_t tos = 0;
    uint8_t ttl = 64;
    bool enableRecvTos = true;
    bool enableRecvTtl = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("tos", "IP_TOS value for sender", tos);
    cmd.AddValue("ttl", "IP_TTL value for sender", ttl);
    cmd.AddValue("recvTos", "Enable IP_RECVTOS on receiver socket", enableRecvTos);
    cmd.AddValue("recvTtl", "Enable IP_RECVTTL on receiver socket", enableRecvTtl);
    cmd.Parse(argc, argv);

    Time interPacketInterval = Seconds(interval);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(10000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(9);
    server.SetServer("UdpServerWithMetadata");
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set socket options on the client socket
    Config::Set("/NodeList/0/ApplicationList/0/SocketIpTos", UintegerValue(tos));
    Config::Set("/NodeList/0/ApplicationList/0/SocketIpTtl", UintegerValue(ttl));

    // Enable IP_RECVTOS and IP_RECVTTL on the server socket
    Config::Set("/NodeList/1/ApplicationList/0/RecvIpTos", BooleanValue(enableRecvTos));
    Config::Set("/NodeList/1/ApplicationList/0/RecvIpTtl", BooleanValue(enableRecvTtl));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}