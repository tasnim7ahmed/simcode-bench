#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelaySimulation");

int main(int argc, char *argv[]) {
    bool enableAnimation = true;
    uint16_t udpPort = 4000;
    std::string dataRate = "10Mbps";
    double interval = 0.005; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableAnimation", "Enable NetAnim output", enableAnimation);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;

    // Server node (node 1)
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Client node (node 0)
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    onoff.SetConstantRate(DataRate("10kbps"), 500);
    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));

    // Relay node (node 2) - no application needed
    // Enable IP forwarding in the relay node's routing table
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (enableAnimation) {
        AnimationInterface anim("udp-relay-animation.xml");
        anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
        anim.SetConstantPosition(nodes.Get(1), 20.0, 0.0);
        anim.SetConstantPosition(nodes.Get(2), 10.0, 10.0);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}