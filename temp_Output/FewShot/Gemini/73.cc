#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer routers;
    routers.Create(3);
    NodeContainer routerA, routerB, routerC;
    routerA.Add(routers.Get(0));
    routerB.Add(routers.Get(1));
    routerC.Add(routers.Get(2));

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install NetDevices
    NetDeviceContainer devicesAB = p2p.Install(routerA.Get(0), routerB.Get(0));
    NetDeviceContainer devicesBC = p2p.Install(routerB.Get(0), routerC.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Addresses for A-B link
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    // Addresses for B-C link
    address.SetBase("10.1.1.4", "255.255.255.252");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    // Global Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup onoff application on A
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer onoffApp = onoff.Install(routerA.Get(0));
    onoffApp.Start(Seconds(1.0));
    onoffApp.Stop(Seconds(10.0));

    // Setup sink application on C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sink.Install(routerC.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable tracing
    p2p.EnablePcapAll("router");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}