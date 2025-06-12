#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTcpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100000000)));
    csma.SetChannelAttribute("PropagationDelay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 8080;

    // Server node (node 0)
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client nodes (nodes 1, 2, 3)
    for (uint32_t i = 1; i < 4; ++i) {
        OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
        client.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));
    }

    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> pcap = pcapHelper.CreateFile("csma_tcp_simulation.pcap", std::ios::out, PcapHelper::DLT_EN10MB);
    for (NetDeviceContainer::Iterator device = devices.Begin(); device != devices.End(); ++device) {
        (*device)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&PcapHelper::Write, pcap));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}