#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-poisson-reverse-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRouting : public Object {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("PoissonReverseRouting")
            .SetParent<Object>()
            .AddConstructor<PoissonReverseRouting>();
        return tid;
    }

    void Setup(Ptr<Node> node, Ptr<NetDevice> dev, Ipv4Address dst, Time updateInterval) {
        m_node = node;
        m_dev = dev;
        m_dst = dst;
        m_updateInterval = updateInterval;
        ScheduleUpdate();
    }

private:
    void ScheduleUpdate() {
        Simulator::Schedule(m_updateInterval, &PoissonReverseRouting::UpdateRoute, this);
    }

    void UpdateRoute() {
        Ipv4StaticRoutingHelper helper;
        Ptr<Ipv4ListRouting> listRouting = m_node->GetObject<Ipv4>()->GetRoutingProtocol()->GetObject<Ipv4ListRouting>();
        if (!listRouting) {
            NS_LOG_ERROR("No list routing protocol found.");
            return;
        }

        double distance = CalculateDistance(); // Simplified metric
        double cost = 1.0 / (distance + 1e-6); // Inverse distance as cost

        for (uint32_t i = 0; i < listRouting->GetNRoutingProtocols(); ++i) {
            int16_t priority;
            Ptr<Ipv4RoutingProtocol> proto = listRouting->GetRoutingProtocol(i, priority);
            if (proto->GetInstanceTypeId().GetName() == "ns3::Ipv4StaticRouting") {
                Ptr<Ipv4StaticRouting> staticRouting = DynamicCast<Ipv4StaticRouting>(proto);
                staticRouting->RemoveAllRoutes();
                staticRouting->AddHostRouteTo(m_dst, m_dev->GetIfIndex(), cost);
                NS_LOG_INFO("Route updated to " << m_dst << " via device " << m_dev->GetIfIndex() << " with cost " << cost);
                break;
            }
        }

        ScheduleUpdate();
    }

    double CalculateDistance() {
        // Simplified fixed distance model between nodes
        return 1.0; // Assume unit distance for point-to-point
    }

    Ptr<Node> m_node;
    Ptr<NetDevice> m_dev;
    Ipv4Address m_dst;
    Time m_updateInterval;
};

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;
    Ipv4StaticRoutingHelper staticRh;
    listRH.Add(staticRh, 0);

    Ipv4PoissonReverseHelper poissonRh;
    listRH.Add(poissonRh, 10);

    stack.SetRoutingHelper(listRH);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    PoissonReverseRouting prRouting;
    prRouting.Setup(nodes.Get(0), devices.Get(0), interfaces.GetAddress(1), Seconds(1.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}