#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ObjectNameExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);
    Names::Add("node0", nodes.Get(0));
    Names::Add("node1", nodes.Get(1));
    Names::Add("node2", nodes.Get(2));
    Names::Add("node3", nodes.Get(3));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices = csma.Install(nodes);
    Names::Add("csma-device-0", devices.Get(0));
    Names::Add("csma-device-1", devices.Get(1));
    Names::Add("csma-device-2", devices.Get(2));
    Names::Add("csma-device-3", devices.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Rename node1 to "client" and node2 to "server"
    Names::Rename("node1", "client");
    Names::Rename("node2", "server");

    // Install applications on the renamed nodes
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("client"));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("object-names-example.tr"));
    csma.EnablePcapAll("object-names-example", false);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}