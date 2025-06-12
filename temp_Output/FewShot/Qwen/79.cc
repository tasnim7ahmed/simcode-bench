#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer n0, n1, n2, n3;
    n0.Create(1);
    n1.Create(1);
    n2.Create(1);
    n3.Create(1);

    // Links between n0-n2 and n1-n2
    PointToPointHelper p2p_n0n2;
    p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link between n1-n3 (higher cost)
    PointToPointHelper p2p_n1n3;
    p2p_n1n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n1n3.SetChannelAttribute("Delay", StringValue("100ms"));

    // Alternate link between n2-n3
    PointToPointHelper p2p_n2n3_alt;
    p2p_n2n3_alt.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n2n3_alt.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    // n0 - n2 link
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer dev_n0n2 = p2p_n0n2.Install(n0.Get(0), n2.Get(0));
    Ipv4InterfaceContainer if_n0n2 = address.Assign(dev_n0n2);
    address.NewNetwork();

    // n1 - n2 link
    NetDeviceContainer dev_n1n2 = p2p_n1n2.Install(n1.Get(0), n2.Get(0));
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);
    address.NewNetwork();

    // n1 - n3 link
    NetDeviceContainer dev_n1n3 = p2p_n1n3.Install(n1.Get(0), n3.Get(0));
    Ipv4InterfaceContainer if_n1n3 = address.Assign(dev_n1n3);
    address.NewNetwork();

    // n2 - n3 alternate link
    NetDeviceContainer dev_n2n3_alt = p2p_n2n3_alt.Install(n2.Get(0), n3.Get(0));
    Ipv4InterfaceContainer if_n2n3_alt = address.Assign(dev_n2n3_alt);

    // Configure static routing with custom metric for n1 to n3
    Ipv4StaticRoutingHelper routingHelper;

    // Get routing tables
    Ptr<Ipv4> ipv4_n1 = n1.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_n1 = routingHelper.GetStaticRouting(ipv4_n1);

    // Default route from n1 to n3 via n2 (lower metric on alternate path)
    Ipv4Address target_n3 = if_n2n3_alt.GetAddress(1);  // n3 side of n2-n3 link
    Ipv4Address nextHop_n2 = if_n1n2.GetAddress(0);     // n2 side of n1-n2 link
    routing_n1->AddHostRouteTo(target_n3, nextHop_n2, 1); // metric = 1

    // Add route from n3 to n1 via n2
    Ptr<Ipv4> ipv4_n3 = n3.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_n3 = routingHelper.GetStaticRouting(ipv4_n3);
    Ipv4Address target_n1 = if_n1n3.GetAddress(0);  // n1 side of n1-n3 link
    Ipv4Address nextHop_n2_n3 = if_n2n3_alt.GetAddress(0); // n2's interface to n3
    routing_n3->AddHostRouteTo(target_n1, nextHop_n2_n3, 1); // metric = 1

    // Set up UDP Echo Server on n1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(n1.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on n3
    UdpEchoClientHelper echoClient(if_n1n3.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(n3.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("network-trace.tr");

    p2p_n0n2.EnableAsciiAll(stream);
    p2p_n1n2.EnableAsciiAll(stream);
    p2p_n1n3.EnableAsciiAll(stream);
    p2p_n2n3_alt.EnableAsciiAll(stream);

    p2p_n0n2.EnablePcapAll("network-pcap");
    p2p_n1n2.EnablePcapAll("network-pcap");
    p2p_n1n3.EnablePcapAll("network-pcap");
    p2p_n2n3_alt.EnablePcapAll("network-pcap");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}