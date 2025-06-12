#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for TcpEchoServer and TcpEchoClient
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Ethernet link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the TCP Server (PacketSink) on node 1
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer serverApp = sinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client (OnOffApplication) on node 0
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    OnOffHelper client("ns3::TcpSocketFactory", Address());
    client.SetAttribute("Remote", remoteAddress);
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("DataRate", StringValue("8192kbps")); // Data rate is required but on TCP acts as upper bound

    // Send packets with OnTime=Constant(0.001), OffTime=Constant(0.099) to get a packet every 0.1s
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.099]"));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}