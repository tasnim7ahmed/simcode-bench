#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfLinkStateRoutingSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Create nodes (routers)
    NS_LOG_INFO("Creating nodes (routers).");
    NodeContainer routers;
    routers.Create(4);

    // Assign node IDs for easier reference in logs
    routers.Get(0)->SetId(0);
    routers.Get(1)->SetId(1);
    routers.Get(2)->SetId(2);
    routers.Get(3)->SetId(3);

    // Create point-to-point links between routers
    NS_LOG_INFO("Creating point-to-point links between routers.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer link01 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer link12 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer link13 = p2p.Install(routers.Get(1), routers.Get(3));
    NetDeviceContainer link02 = p2p.Install(routers.Get(0), routers.Get(2));

    // Install Internet stack with OSPF routing
    NS_LOG_INFO("Installing internet stack and OSPF routing protocol.");
    InternetStackHelper stack;
    OspfHelper ospfRouting;
    stack.SetRoutingHelper(ospfRouting);  // Use OSPF
    stack.Install(routers);

    // Assign IP addresses to interfaces
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifc01 = ipv4.Assign(link01);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifc12 = ipv4.Assign(link12);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifc13 = ipv4.Assign(link13);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifc02 = ipv4.Assign(link02);

    // Create source and destination nodes (hosts)
    NS_LOG_INFO("Creating host nodes.");
    NodeContainer hosts;
    hosts.Create(2);

    // Connect host 0 to router 0
    NetDeviceContainer linkHost0Router0 = p2p.Install(hosts.Get(0), routers.Get(0));
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifcHost0Router0 = ipv4.Assign(linkHost0Router0);

    // Connect host 1 to router 3
    NetDeviceContainer linkHost1Router3 = p2p.Install(hosts.Get(1), routers.Get(3));
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifcHost1Router3 = ipv4.Assign(linkHost1Router3);

    // Install internet stack on hosts
    stack.Install(hosts);

    // Set up traffic: UDP echo from host 0 to host 1
    NS_LOG_INFO("Setting up UDP echo application.");
    uint16_t port = 9;  // well-known echo port

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ifcHost1Router3.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable routing tables logging
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing
    if (tracing) {
        NS_LOG_INFO("Enabling pcap tracing.");
        p2p.EnablePcapAll("ospf-link-state-routing");
    }

    // Run simulation
    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}