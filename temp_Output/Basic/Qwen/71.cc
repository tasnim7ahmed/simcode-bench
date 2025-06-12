#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> A = nodes.Get(0);
    Ptr<Node> B = nodes.Get(1);
    Ptr<Node> C = nodes.Get(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB = p2p.Install(A, B);
    NetDeviceContainer devBC = p2p.Install(B, C);

    CsmaHelper csmaA;
    csmaA.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaA.SetChannelAttribute("Delay", StringValue("1ms"));
    CsmaHelper csmaC;
    csmaC.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaC.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devCsmaA = csmaA.Install(A);
    NetDeviceContainer devCsmaC = csmaC.Install(C);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer ifAB = address.Assign(devAB);

    address.SetBase("10.1.2.0", "255.255.255.252");
    Ipv4InterfaceContainer ifBC = address.Assign(devBC);

    address.SetBase("172.16.1.0", "255.255.255.255");
    Ipv4InterfaceContainer ifCsmaA = address.Assign(devCsmaA);

    address.SetBase("192.168.1.0", "255.255.255.255");
    Ipv4InterfaceContainer ifCsmaC = address.Assign(devCsmaC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(C);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ifCsmaC.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(A);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-sim-p2p.tr"));
    csmaA.EnableAsciiAll(ascii.CreateFileStream("three-router-sim-csmaA.tr"));
    csmaC.EnableAsciiAll(ascii.CreateFileStream("three-router-sim-csmaC.tr"));

    p2p.EnablePcapAll("three-router-sim");
    csmaA.EnablePcapAll("three-router-sim-csmaA");
    csmaC.EnablePcapAll("three-router-sim-csmaC");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}