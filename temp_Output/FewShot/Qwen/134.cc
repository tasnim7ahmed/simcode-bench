#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-packet-info-tag.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseRouting");

class PoissonReverseRouting : public Ipv4RoutingProtocol {
public:
    static TypeId GetTypeId();
    PoissonReverseRouting();
    virtual ~PoissonReverseRouting();

    void SetIpv4(Ptr<Ipv4> ipv4) override;
    Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override;
    bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                    UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                    LocalDeliverCallback lcb, ErrorCallback ecb) override;
    void NotifyInterfaceUp(uint32_t interface) override {}
    void NotifyInterfaceDown(uint32_t interface) override {}
    void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override {}
    void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override {}
    void SetNextHop(Ptr<Node> node, Ipv4Address dst, Ipv4Address nextHop, uint32_t interface);

private:
    void UpdateRoutingTable();
    void ScheduleRouteUpdate();
    double ComputeCost(double distance);
    double EstimateDistance(Ptr<Node> a, Ptr<Node> b);

    Ptr<Ipv4> m_ipv4;
    std::map<Ipv4Address, Ipv4Address> m_routingTable; // Destination -> Next Hop
    EventId m_routeUpdateEvent;
};

TypeId PoissonReverseRouting::GetTypeId() {
    static TypeId tid = TypeId("PoissonReverseRouting")
                            .SetParent<Ipv4RoutingProtocol>()
                            .AddConstructor<PoissonReverseRouting>();
    return tid;
}

PoissonReverseRouting::PoissonReverseRouting()
    : m_ipv4(nullptr), m_routeUpdateEvent(Events::none()) {}

PoissonReverseRouting::~PoissonReverseRouting() {}

void PoissonReverseRouting::SetIpv4(Ptr<Ipv4> ipv4) {
    m_ipv4 = ipv4;
    ScheduleRouteUpdate();
}

Ptr<Ipv4Route> PoissonReverseRouting::RouteOutput(Ptr<Packet> p, const Ipv4Header &header,
                                                 Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) {
    NS_LOG_FUNCTION(this << header.GetDestination());
    Ipv4Address dest = header.GetDestination();
    if (m_routingTable.find(dest) != m_routingTable.end()) {
        Ipv4Address nextHop = m_routingTable[dest];
        uint32_t interface = 0; // Assume default interface
        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
            if (m_ipv4->GetAddress(i, 0).GetLocal() == nextHop) {
                interface = i;
                break;
            }
        }
        if (interface >= m_ipv4->GetNInterfaces()) {
            return nullptr;
        }

        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(dest);
        route->SetGateway(nextHop);
        route->SetSource(m_ipv4->GetAddress(interface, 0).GetLocal());
        route->SetOutputDevice(m_ipv4->GetNetDevice(interface));
        return route;
    }
    return nullptr;
}

bool PoissonReverseRouting::RouteInput(Ptr<const Packet> p, const Ipv4Header &header,
                                      Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                                      MulticastForwardCallback mcb, LocalDeliverCallback lcb,
                                      ErrorCallback ecb) {
    NS_LOG_FUNCTION(this << header.GetDestination());
    if (header.GetDestination().IsBroadcast() || header.GetDestination().IsMulticast()) {
        return false;
    }
    Ipv4Address dest = header.GetDestination();
    if (dest == m_ipv4->GetLocalAddress(0)) {
        lcb(p, header, 0);
        return true;
    }
    if (m_routingTable.find(dest) != m_routingTable.end()) {
        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(dest);
        route->SetGateway(m_routingTable[dest]);
        route->SetSource(m_ipv4->GetAddress(0, 0).GetLocal());
        route->SetOutputDevice(m_ipv4->GetNetDevice(0));
        ucb(route, p, header);
        return true;
    }
    return false;
}

void PoissonReverseRouting::SetNextHop(Ptr<Node> node, Ipv4Address dst, Ipv4Address nextHop, uint32_t interface) {
    m_routingTable[dst] = nextHop;
}

double PoissonReverseRouting::ComputeCost(double distance) {
    return 1.0 / (distance + 1e-6); // Inverse of distance to avoid division by zero
}

double PoissonReverseRouting::EstimateDistance(Ptr<Node> a, Ptr<Node> b) {
    // Simulated distance based on node IDs
    return std::abs(static_cast<int>(a->GetId()) - static_cast<int>(b->GetId())) * 10.0;
}

void PoissonReverseRouting::UpdateRoutingTable() {
    NS_LOG_INFO("Updating routing table using Poisson Reverse method");
    for (auto it = m_routingTable.begin(); it != m_routingTable.end(); ++it) {
        Ipv4Address dst = it->first;
        double minCost = std::numeric_limits<double>::max();
        Ipv4Address bestNextHop;

        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
            Ipv4Address neighbor = m_ipv4->GetAddress(i, 0).GetLocal();
            double distance = EstimateDistance(m_ipv4->GetObject<Node>(), Nodes.GetNode(neighbor));
            double cost = ComputeCost(distance);
            if (cost < minCost) {
                minCost = cost;
                bestNextHop = neighbor;
            }
        }

        if (bestNextHop != Ipv4Address()) {
            m_routingTable[dst] = bestNextHop;
            NS_LOG_INFO("Route updated: Destination=" << dst << ", Next Hop=" << bestNextHop << ", Cost=" << minCost);
        }
    }
    ScheduleRouteUpdate();
}

void PoissonReverseRouting::ScheduleRouteUpdate() {
    if (!m_routeUpdateEvent.IsRunning()) {
        m_routeUpdateEvent = Simulator::Schedule(Seconds(5.0), &PoissonReverseRouting::UpdateRoutingTable, this);
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("PoissonReverseRouting", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.AddRoutingProtocol(CreateObject<PoissonReverseRouting>());
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}