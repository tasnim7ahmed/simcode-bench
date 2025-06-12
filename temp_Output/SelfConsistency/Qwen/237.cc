#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpClientServerWithRelay");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClientServerWithRelay", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);  // client, relay, server

    // Set up CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    // Connect client to relay
    NetDeviceContainer link1;
    link1 = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));

    // Connect relay to server
    NetDeviceContainer link2;
    link2 = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = ipv4.Assign(link1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = ipv4.Assign(link2);

    // Enable routing on the relay node (node 1)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP server on node 2 (server)
    UdpServerHelper server(9);  // port 9
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on node 0 (client)
    UdpClientHelper client(interfaces2.GetAddress(1), 9);  // server's address and port
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging for received packets at server
    Config::Connect("/NodeList/2/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> p, const Address &) {
        NS_LOG_INFO("Server received packet of size: " << p->GetSize());
    }));

    // Setup animation
    AnimationInterface anim("udp-client-server-relay.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 50, 0);
    anim.SetConstantPosition(nodes.Get(2), 100, 0);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}