#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

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

    void SetIpv4(Ptr<Ipv4> ipv4) {
        m_ipv4 = ipv4;
    }

    void UpdateRoutes() {
        NS_LOG_INFO("Updating routes based on inverse distance metric.");
        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
            Ipv4InterfaceAddress ifAddr = m_ipv4->GetAddress(i, 0);
            Ipv4Address dest = ifAddr.GetLocal();
            double cost = 1.0 / (std::max(1.0, CalculateDistance(dest)));
            NS_LOG_INFO("Setting route to " << dest << " with cost " << cost);
            m_staticRoute->SetDefaultRoute(dest, i, cost);
        }
    }

private:
    double CalculateDistance(Ipv4Address dest) {
        // Simulated distance: use a deterministic function of IP address value
        uint32_t ip = dest.Get();
        return static_cast<double>((ip & 0xFF) + ((ip >> 8) & 0xFF));
    }

    Ptr<Ipv4StaticRouting> m_staticRoute = CreateObject<Ipv4StaticRouting>();
    Ptr<Ipv4> m_ipv4;
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
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ptr<PoissonReverseRouting> prRouting = CreateObject<PoissonReverseRouting>();
    prRouting->SetIpv4(nodes.Get(0)->GetObject<Ipv4>());
    prRouting->SetIpv4(nodes.Get(1)->GetObject<Ipv4>());

    Simulator::ScheduleRepeating(Seconds(2), &PoissonReverseRouting::UpdateRoutes, prRouting);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}