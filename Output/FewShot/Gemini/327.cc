#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_ALL);

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.Install(nodes);

    // Internet Stack
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // Assign Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

    // UDP Echo Client/Server
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(5), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Animation Interface
    AnimationInterface anim("manet-olsr.xml");

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}