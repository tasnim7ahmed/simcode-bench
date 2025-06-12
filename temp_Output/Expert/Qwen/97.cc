#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/object-name-service.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ObjectNameServiceExample");

int
main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);
    Names::Add("client", nodes.Get(0));
    Names::Add("server", nodes.Get(1));

    // Rename node 0 for demonstration
    Names::Rename("client", "frontend");
    Names::Add("switch", nodes.Get(2));
    Names::Add("router", nodes.Get(3));

    // Setup CSMA
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "device-" << i;
        Names::Add(oss.str(), devices.Get(i));
    }

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP echo server on "server" node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo client on renamed "frontend" node
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("frontend"));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    csma.EnablePcapAll("object-names-example");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}