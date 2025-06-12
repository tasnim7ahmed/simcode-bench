#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t next = (i + 1) % 4;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(next));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "192.9.39." << i * 64 << "/26";
        address.SetBase(subnet.str().c_str());
        interfaces[i] = address.Assign(devices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t serverPort = 9;
    ApplicationContainer serverApps, clientApps;

    // First pair: node 0 as client, node 1 as server
    UdpEchoServerHelper echoServer(serverPort);
    serverApps.Add(echoServer.Install(nodes.Get(1)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[1].GetAddress(1), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient.Install(nodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Second pair: node 1 as client, node 2 as server
    serverApps.Clear();
    serverApps.Add(echoServer.Install(nodes.Get(2)));
    serverApps.Start(Seconds(11.0));
    serverApps.Stop(Seconds(20.0));

    clientApps.Clear();
    echoClient = UdpEchoClientHelper(interfaces[2].GetAddress(1), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient.Install(nodes.Get(1)));
    clientApps.Start(Seconds(12.0));
    clientApps.Stop(Seconds(20.0));

    // Third pair: node 2 as client, node 3 as server
    serverApps.Clear();
    serverApps.Add(echoServer.Install(nodes.Get(3)));
    serverApps.Start(Seconds(21.0));
    serverApps.Stop(Seconds(30.0));

    clientApps.Clear();
    echoClient = UdpEchoClientHelper(interfaces[3].GetAddress(1), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient.Install(nodes.Get(2)));
    clientApps.Start(Seconds(22.0));
    clientApps.Stop(Seconds(30.0));

    // Fourth pair: node 3 as client, node 0 as server
    serverApps.Clear();
    serverApps.Add(echoServer.Install(nodes.Get(0)));
    serverApps.Start(Seconds(31.0));
    serverApps.Stop(Seconds(40.0));

    clientApps.Clear();
    echoClient = UdpEchoClientHelper(interfaces[0].GetAddress(1), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient.Install(nodes.Get(3)));
    clientApps.Start(Seconds(32.0));
    clientApps.Stop(Seconds(40.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}