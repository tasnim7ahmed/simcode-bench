#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Set up CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Allocate IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server (PacketSink) on node 0
    uint16_t port = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP clients (OnOffApplication) on nodes 1,2,3
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    for (uint32_t i = 1; i < 4; ++i) {
        OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
        clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(i));
    }

    // Enable PCAP tracing
    csma.EnablePcapAll("csma-tcp");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}