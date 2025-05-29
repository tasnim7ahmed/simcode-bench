#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetwork");

int main(int argc, char* argv[]) {
    LogComponent::SetAttribute("UdpClient", "Interval", StringValue("0.01"));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);
    Names::Add("A", nodes.Get(0));
    Names::Add("B", nodes.Get(1));
    Names::Add("C", nodes.Get(2));

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Create Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer csmaDevicesA = csma.Install(nodes.Get(0));
    NetDeviceContainer csmaDevicesC = csma.Install(nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252"); // /30 subnet for A-B
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.1.4", "255.255.255.252"); // /30 subnet for B-C
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfacesA = address.Assign(csmaDevicesA);
    csmaInterfacesA.GetAddress(0); // get address for node A (172.16.1.1 by default)
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfacesC = address.Assign(csmaDevicesC);
    csmaInterfacesC.GetAddress(0); // get address for node C (192.168.1.1 by default)

    // Assign /32 addresses for A and C over point-to-point
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeC = nodes.Get(2);

    Ipv4InterfaceAddress ifaceA = Ipv4InterfaceAddress(Ipv4Address("1.1.1.1"), Ipv4Mask("255.255.255.255"));
    Ipv4InterfaceAddress ifaceC = Ipv4InterfaceAddress(Ipv4Address("3.3.3.3"), Ipv4Mask("255.255.255.255"));

    uint32_t interfaceIndexA = nodeA->GetNDevices()-1;
    uint32_t interfaceIndexC = nodeC->GetNDevices()-1;

    devicesAB.Get(0)->SetAddress(Mac48Address::Allocate());
    devicesBC.Get(1)->SetAddress(Mac48Address::Allocate());
    csmaDevicesA.Get(0)->SetAddress(Mac48Address::Allocate());
    csmaDevicesC.Get(0)->SetAddress(Mac48Address::Allocate());

    devicesAB.Get(0)->SetIfIndex(interfaceIndexA);
    devicesBC.Get(1)->SetIfIndex(interfaceIndexC);

    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();

    ipv4A->SetAddress(interfaceIndexA, ifaceA);
    ipv4A->SetMetric(interfaceIndexA, 1);
    ipv4A->SetForwarding(interfaceIndexA, true);
    ipv4C->SetAddress(interfaceIndexC, ifaceC);
    ipv4C->SetMetric(interfaceIndexC, 1);
    ipv4C->SetForwarding(interfaceIndexC, true);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP application
    uint16_t port = 9;  // Discard port (RFC 863)

    UdpClientHelper client(interfacesBC.GetAddress(1), port); // Address of C
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Install on A
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2)); // Install on C
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Enable tracing
    pointToPoint.EnablePcapAll("three-router");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}