#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

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
    csma.SetChannelAttribute("PropagationDelay", TimeValue(Time("6560ns")));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 8080;

    // TCP Server on node 0
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // TCP Clients on nodes 1, 2, 3
    for (uint32_t i = 1; i < 4; ++i) {
        InetSocketAddress remoteAddress(interfaces.GetAddress(0), port);
        remoteAddress.SetTos(0xb8); // AF41 - Just example traffic class

        BulkSendHelper clientHelper("ns3::TcpSocketFactory", remoteAddress);
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing
    csma.EnablePcapAll("csma_tcp_simulation", false);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}