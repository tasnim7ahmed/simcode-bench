#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DVRSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(false));
    Config::SetDefault("ns3::DvRoutingProtocol::PeriodicUpdateInterval", TimeValue(Seconds(15)));
    Config::SetDefault("ns3::DvRoutingProtocol::InvalidationTimeout", TimeValue(Seconds(60)));

    NodeContainer routers;
    routers.Create(3);

    NodeContainer subnets;
    subnets.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[3][3];
    Ipv4InterfaceContainer routerInterfaces[3][3];

    InternetStackHelper internet;
    DvRoutingHelper dv;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(dv, 10);

    internet.SetRoutingHelper(listRH);
    internet.Install(routers);

    // Connect routers in a triangle: R0 <-> R1 <-> R2 <-> R0
    routerDevices[0][1] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    routerDevices[1][2] = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2)));
    routerDevices[2][0] = p2p.Install(NodeContainer(routers.Get(2), routers.Get(0)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    routerInterfaces[0][1] = ipv4.Assign(routerDevices[0][1]);
    ipv4.NewNetwork();
    routerInterfaces[1][2] = ipv4.Assign(routerDevices[1][2]);
    ipv4.NewNetwork();
    routerInterfaces[2][0] = ipv4.Assign(routerDevices[2][0]);
    ipv4.NewNetwork();

    // Subnet connections to routers
    NetDeviceContainer subnetDevices[4];
    Ipv4InterfaceContainer subnetInterfaces[4];

    internet.Install(subnets);

    // Subnet 0 connected to Router 0
    subnetDevices[0] = p2p.Install(NodeContainer(routers.Get(0), subnets.Get(0)));
    ipv4.SetBase("192.168.0.0", "255.255.255.0");
    subnetInterfaces[0] = ipv4.Assign(subnetDevices[0]);
    ipv4.NewNetwork();

    // Subnet 1 connected to Router 1
    subnetDevices[1] = p2p.Install(NodeContainer(routers.Get(1), subnets.Get(1)));
    subnetInterfaces[1] = ipv4.Assign(subnetDevices[1]);
    ipv4.NewNetwork();

    // Subnet 2 connected to Router 2
    subnetDevices[2] = p2p.Install(NodeContainer(routers.Get(2), subnets.Get(2)));
    subnetInterfaces[2] = ipv4.Assign(subnetDevices[2]);
    ipv4.NewNetwork();

    // Subnet 3 connected to Router 0
    subnetDevices[3] = p2p.Install(NodeContainer(routers.Get(0), subnets.Get(3)));
    subnetInterfaces[3] = ipv4.Assign(subnetDevices[3]);

    // Enable pcap tracing on all devices
    p2p.EnablePcapAll("dvr_simulation");

    // Setup traffic between subnetworks
    uint16_t port = 9;

    // Traffic from subnet 0 to subnet 1
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(subnets.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(subnetInterfaces[1].GetAddress(1), port));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(subnets.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Traffic from subnet 2 to subnet 3
    port += 1;
    sinkLocalAddress = Address(InetSocketAddress(Ipv4Address::GetAny(), port));
    packetSinkHelper = PacketSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    sinkApp = packetSinkHelper.Install(subnets.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    clientHelper = OnOffHelper("ns3::TcpSocketFactory", InetSocketAddress(subnetInterfaces[3].GetAddress(1), port));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    clientApp = clientHelper.Install(subnets.Get(2));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}