#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2pLowLatency;
    p2pLowLatency.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pLowLatency.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pHighCost;
    p2pHighCost.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2pHighCost.SetChannelAttribute("Delay", StringValue("100ms"));

    PointToPointHelper p2pAltPath;
    p2pAltPath.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2pAltPath.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer dev_n0_n2 = p2pLowLatency.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev_n1_n2 = p2pLowLatency.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev_n1_n3 = p2pHighCost.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer dev_n2_n3 = p2pAltPath.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0_n2 = address.Assign(dev_n0_n2);

    address.NewNetwork();
    Ipv4InterfaceContainer if_n1_n2 = address.Assign(dev_n1_n2);

    address.NewNetwork();
    Ipv4InterfaceContainer if_n1_n3 = address.Assign(dev_n1_n3);

    address.NewNetwork();
    Ipv4InterfaceContainer if_n2_n3 = address.Assign(dev_n2_n3);

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4_n1 = nodes.Get(1)->GetObject<Ipv4>();
    uint32_t if_index_n1_n2 = ipv4_n1->GetInterfaceForDevice(dev_n1_n2.Get(0));
    uint32_t if_index_n1_n3 = ipv4_n1->GetInterfaceForDevice(dev_n1_n3.Get(0));
    ipv4_n1->SetMetric(if_index_n1_n2, 1);
    ipv4_n1->SetMetric(if_index_n1_n3, 10); // Configurable metric for rerouting

    Ptr<Ipv4> ipv4_n3 = nodes.Get(3)->GetObject<Ipv4>();
    uint32_t if_index_n3_n1 = ipv4_n3->GetInterfaceForDevice(dev_n1_n3.Get(1));
    uint32_t if_index_n3_n2 = ipv4_n3->GetInterfaceForDevice(dev_n2_n3.Get(1));
    ipv4_n3->SetMetric(if_index_n3_n1, 10); // Same as above
    ipv4_n3->SetMetric(if_index_n3_n2, 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(if_n1_n2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("network-topology-simulation.tr");
    p2pLowLatency.EnableAsciiAll(stream);
    p2pHighCost.EnableAsciiAll(stream);
    p2pAltPath.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}