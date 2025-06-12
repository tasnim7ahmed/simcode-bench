#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpEcho");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::Ipv4GlobalRouting::PerflowEcmp", BooleanValue(true));
    uint32_t packetCount = 4;
    Time interPacketInterval = Seconds(1.0);
    Time simulationDuration = Seconds(20.0);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t j = (i + 1) % 4;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "192.9.39." << (i * 64) << "/26";
        address.SetBase(subnet.str().c_str(), "255.255.255.192");
        interfaces[i] = address.Assign(devices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t serverNodeIndex = 0;
    uint32_t clientNodeIndex = 1;
    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(serverNodeIndex));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simulationDuration);

    UdpEchoClientHelper echoClient(interfaces[clientNodeIndex].GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(clientNodeIndex));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(simulationDuration);

    Simulator::Stop(simulationDuration);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}