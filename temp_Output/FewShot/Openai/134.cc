#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-list-routing-helper.h"
#include <map>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRoutingProtocol : public Ipv4RoutingProtocol
{
public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("PoissonReverseRoutingProtocol")
            .SetParent<Ipv4RoutingProtocol>()
            .SetGroupName("Internet")
            .AddConstructor<PoissonReverseRoutingProtocol>();
        return tid;
    }

    PoissonReverseRoutingProtocol()
    {
        m_updateInterval = Seconds(2.0);
    }
    virtual ~PoissonReverseRoutingProtocol() {}

    void SetNode(Ptr<Node> node)
    {
        m_node = node;
    }

    void SetIpv4(Ptr<Ipv4> ipv4)
    {
        m_ipv4 = ipv4;
    }

    void SetDeviceToNeighbor(const std::map<Ptr<NetDevice>, Ipv4Address>& map)
    {
        m_devToNeigh = map;
    }

    void Start()
    {
        // Initialize routes to own interfaces
        auto nIfaces = m_ipv4->GetNInterfaces();
        for (uint32_t i = 0; i < nIfaces; ++i)
        {
            Ipv4Address addr = m_ipv4->GetAddress(i,0).GetLocal();
            if (addr != Ipv4Address("127.0.0.1"))
            {
                m_routeTable[addr] = {0.0, 0}; // cost 0, interface index
            }
        }
        Simulator::Schedule(Seconds(0.1), &PoissonReverseRoutingProtocol::SendRouteUpdate, this);
        Simulator::Schedule(m_updateInterval, &PoissonReverseRoutingProtocol::PeriodicUpdate, this);
    }

    struct RouteEntry
    {
        double cost;
        uint32_t interfaceIdx;
    };

    virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> packet, const Ipv4Header &header,
                                       Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
        Ipv4Address dst = header.GetDestination();
        Ptr<Ipv4Route> rt = nullptr;
        auto it = m_routeTable.find(dst);
        if (it != m_routeTable.end())
        {
            rt = Create<Ipv4Route>();
            rt->SetDestination(dst);
            rt->SetGateway(Ipv4Address("0.0.0.0"));
            rt->SetOutputDevice(m_ipv4->GetNetDevice(it->second.interfaceIdx));
            rt->SetSource(m_ipv4->GetAddress(it->second.interfaceIdx,0).GetLocal());
            sockerr = Socket::ERROR_NOTERROR;
        }
        return rt;
    }

    virtual bool RouteInput(Ptr<const Packet> packet, const Ipv4Header &header, Ptr<const NetDevice> idev,
                            UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb,
                            ErrorCallback ecb) override
    {
        Ipv4Address dst = header.GetDestination();
        Ipv4Address src = header.GetSource();
        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i)
        {
            for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j)
            {
                if (m_ipv4->GetAddress(i,j).GetLocal() == dst)
                {
                    lcb(packet, header, i);
                    return true;
                }
            }
        }
        auto it = m_routeTable.find(dst);
        if (it != m_routeTable.end())
        {
            uint32_t oidx = it->second.interfaceIdx;
            Ptr<NetDevice> outDevice = m_ipv4->GetNetDevice(oidx);
            ucb(outDevice, header, packet, src, dst, 0);
            return true;
        }
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }

    virtual void NotifyInterfaceUp(uint32_t i) override {}
    virtual void NotifyInterfaceDown(uint32_t i) override {}
    virtual void NotifyAddAddress(uint32_t i, Ipv4InterfaceAddress address) override {}
    virtual void NotifyRemoveAddress(uint32_t i, Ipv4InterfaceAddress address) override {}
    virtual void SetIpv4(Ptr<Ipv4> ipv4) override
    {
        m_ipv4 = ipv4;
    }
    virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override
    {
        *stream->GetStream() << "Node " << m_node->GetId() << " PoissonReverseRouting Table:\n";
        for (auto &e : m_routeTable)
        {
            *stream->GetStream() << "  " << e.first << " via if " << e.second.interfaceIdx
                                << " cost " << std::fixed << std::setprecision(4) << e.second.cost << "\n";
        }
    }

    void SendRouteUpdate()
    {
        for (auto &dev_neigh : m_devToNeigh)
        {
            Ptr<Socket> s;
            if (m_udpSockets.find(dev_neigh.first) == m_udpSockets.end())
            {
                s = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());
                s->BindToNetDevice(dev_neigh.first);
                m_udpSockets[dev_neigh.first] = s;
            }
            else
                s = m_udpSockets[dev_neigh.first];
            Ipv4Address dstIp = dev_neigh.second;
            Ptr<Packet> p = Create<Packet>();
            // Put number of entries as first byte
            uint8_t nEntries = m_routeTable.size();
            p->AddHeader(Buffer::Iterator::WriteU8(nEntries));
            for (auto &e : m_routeTable)
            {
                // dst, cost
                uint32_t addr = e.first.Get();
                double cost = e.second.cost;
                uint8_t buf[8];
                memcpy(buf, &addr, 4);
                memcpy(buf+4, &cost, 4);
                p->AddAtEnd(Create<Packet>(buf, 8));
            }
            InetSocketAddress dstSockAddr(dstIp, 54321);
            s->SendTo(p, 0, dstSockAddr);
        }
    }

    void ReceiveRouteUpdate(Ptr<Socket> socket)
    {
        Address from;
        Ptr<Packet> pkt = socket->RecvFrom(from);
        uint8_t len;
        pkt->CopyData(&len, 1);
        Buffer::Iterator iter = pkt->Begin();
        iter.Next(1);
        for (uint8_t i = 0; i < len; ++i)
        {
            uint32_t addr;
            float rcost;
            pkt->CopyData((uint8_t*)&addr, 4);
            pkt->RemoveAtStart(4);
            pkt->CopyData((uint8_t*)&rcost, 4);
            pkt->RemoveAtStart(4);
            Ipv4Address dst(addr);
            // Suppose we know our own interface distance (1), invert to get cost
            double distance = 1.0; // point-to-point, can be measured or set dynamically
            double newCost = 1.0 / (distance + 1.0 / (rcost + 1e-6));
            // Poisson Reverse: Only update if cost improves
            if (m_routeTable.find(dst)==m_routeTable.end() || m_routeTable[dst].cost > newCost)
            {
                NS_LOG_INFO("Node " << m_node->GetId()
                    << " updated route to " << dst
                    << " with cost " << std::fixed << std::setprecision(4) << newCost);
                // Deduce incoming interface
                Ptr<NetDevice> srcDevice;
                for (auto it = m_devToNeigh.begin(); it != m_devToNeigh.end(); ++it)
                    if (InetSocketAddress::ConvertFrom(from).GetIpv4() == it->second)
                        srcDevice = it->first;
                uint32_t ifaceIdx = 0;
                for (uint32_t j = 0; j < m_ipv4->GetNInterfaces(); ++j)
                    if (m_ipv4->GetNetDevice(j) == srcDevice)
                        ifaceIdx = j;
                m_routeTable[dst] = { newCost, ifaceIdx };
            }
        }
    }

    void PeriodicUpdate()
    {
        SendRouteUpdate();
        Simulator::Schedule(m_updateInterval, &PoissonReverseRoutingProtocol::PeriodicUpdate, this);
    }

    void SetupReceive()
    {
        for (auto &dev_neigh : m_devToNeigh)
        {
            Ptr<Socket> s = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());
            s->BindToNetDevice(dev_neigh.first);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 54321);
            s->Bind(local);
            s->SetRecvCallback(MakeCallback(&PoissonReverseRoutingProtocol::ReceiveRouteUpdate, this));
        }
    }

    std::map<Ipv4Address, RouteEntry> m_routeTable;
    std::map<Ptr<NetDevice>, Ipv4Address> m_devToNeigh;
    std::map<Ptr<NetDevice>, Ptr<Socket>> m_udpSockets;
    Ptr<Node> m_node;
    Ptr<Ipv4> m_ipv4;
    Time m_updateInterval;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;

    // We will not install standard routing! Install only our minimal protocol
    // So: Build bare IPv4 stack.
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
        stack.Install(nodes.Get(i));

    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = addr.Assign(devices);

    // Build dev<->neighbor mapping for our protocol
    std::vector< Ptr<PoissonReverseRoutingProtocol> > prProtocols(2);
    for (uint32_t i = 0; i < 2; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        std::map<Ptr<NetDevice>, Ipv4Address> devNeigh;
        Ptr<NetDevice> nd = devices.Get(i);
        Ipv4Address neighAddr = ifaces.GetAddress(1-i);
        devNeigh[nd] = neighAddr;

        prProtocols[i] = CreateObject<PoissonReverseRoutingProtocol>();
        prProtocols[i]->SetNode(node);
        prProtocols[i]->SetIpv4(ipv4);
        prProtocols[i]->SetDeviceToNeighbor(devNeigh);
        ipv4->SetRoutingProtocol(prProtocols[i]);
        prProtocols[i]->SetupReceive();
    }
    prProtocols[0]->Start();
    prProtocols[1]->Start();

    // Applications: UDP source/sink
    uint16_t port = 44444;

    UdpServerHelper server(port);
    ApplicationContainer appS = server.Install(nodes.Get(1));
    appS.Start(Seconds(1.0));
    appS.Stop(Seconds(14.0));

    UdpClientHelper client(ifaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer appC = client.Install(nodes.Get(0));
    appC.Start(Seconds(3.0));
    appC.Stop(Seconds(13.0));

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    // Print out statistics
    Ptr<UdpServer> serv = appS.Get(0)->GetObject<UdpServer>();
    std::cout << "Packets received at Server: " << serv->GetReceived() << std::endl;
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    prProtocols[0]->PrintRoutingTable(stream);
    prProtocols[1]->PrintRoutingTable(stream);

    Simulator::Destroy();
    return 0;
}