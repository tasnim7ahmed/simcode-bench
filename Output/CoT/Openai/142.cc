#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("StarTopologyExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    // Naming nodes: 0=center, 1 & 2 = peripherals
    Ptr<Node> center = nodes.Get(0);
    Ptr<Node> peripheral1 = nodes.Get(1);
    Ptr<Node> peripheral2 = nodes.Get(2);

    // Create point-to-point links: center <-> peripheral1, center <-> peripheral2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Link 1: center <-> peripheral1
    NodeContainer link1(center, peripheral1);
    NetDeviceContainer devices1 = p2p.Install(link1);

    // Link 2: center <-> peripheral2
    NodeContainer link2(center, peripheral2);
    NetDeviceContainer devices2 = p2p.Install(link2);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(devices1);

    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address2.Assign(devices2);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Deploy UDP Echo Server on peripheral2
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(peripheral2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Deploy UDP Echo Client on peripheral1 targeting peripheral2's address
    UdpEchoClientHelper echoClient(interfaces2.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(peripheral1);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("star-topology.tr"));
    p2p.EnablePcapAll("star-topology");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}