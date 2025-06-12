#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.AddValue("enableDCTCP", "Enable DCTCP", false);
    cmd.AddValue("pcap", "Enable PCAP tracing", false);
    cmd.Parse(argc, argv);

    bool enableDCTCP = false;
    bool enablePcap = false;

    cmd.Parse(argc, argv);

    // Nodes
    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(2);

    NodeContainer routerNodes;
    routerNodes.Create(2);

    // PointToPoint Links
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("20ms"));

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install NetDevices
    NetDeviceContainer serverRouterDevices = accessLink.Install(serverNode.Get(0), routerNodes.Get(1));
    NetDeviceContainer clientRouterDevices1 = accessLink.Install(clientNodes.Get(0), routerNodes.Get(0));
    NetDeviceContainer clientRouterDevices2 = accessLink.Install(clientNodes.Get(1), routerNodes.Get(0));
    NetDeviceContainer routerRouterDevices = bottleneckLink.Install(routerNodes.Get(0), routerNodes.Get(1));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(clientNodes);
    stack.Install(routerNodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverRouterIfaces = address.Assign(serverRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterIfaces1 = address.Assign(clientRouterDevices1);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterIfaces2 = address.Assign(clientRouterDevices2);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer routerRouterIfaces = address.Assign(routerRouterDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic Control
    TrafficControlHelper tchBottleneck;
    tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    QueueDiscContainer queueDiscsBottleneck = tchBottleneck.Install(routerRouterDevices);

    TrafficControlHelper tchAccess;
    tchAccess.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    QueueDiscContainer queueDiscsAccess1 = tchAccess.Install(clientRouterDevices1);
    QueueDiscContainer queueDiscsAccess2 = tchAccess.Install(clientRouterDevices2);
    QueueDiscContainer queueDiscsAccess3 = tchAccess.Install(serverRouterDevices);

    // Applications
    uint16_t port = 5000;
    BulkSendHelper bulkSendClient1("ns3::TcpSocketFactory", InetSocketAddress(serverRouterIfaces.GetAddress(0), port));
    bulkSendClient1.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApps1 = bulkSendClient1.Install(clientNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    BulkSendHelper bulkSendClient2("ns3::TcpSocketFactory", InetSocketAddress(serverRouterIfaces.GetAddress(0), port+1));
    bulkSendClient2.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApps2 = bulkSendClient2.Install(clientNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(10.0));

    PacketSinkHelper serverSink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp1 = serverSink1.Install(serverNode.Get(0));
    serverApp1.Start(Seconds(0.0));
    serverApp1.Stop(Seconds(10.0));

    PacketSinkHelper serverSink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port+1));
    ApplicationContainer serverApp2 = serverSink2.Install(serverNode.Get(0));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(10.0));


    PingHelper ping(serverRouterIfaces.GetAddress(0));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer pingApps = ping.Install(clientNodes.Get(0));
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(10.0));

    // Tracing
    if (enablePcap) {
        bottleneckLink.EnablePcapAll("scratch/pcap_trace");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}