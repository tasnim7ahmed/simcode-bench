#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    bool enableTracing = false;
    bool useNanosecPcap = false;

    CommandLine cmd;
    cmd.AddValue("EnableTracing", "Enable packet tracing (pcap)", enableTracing);
    cmd.AddValue("UseNanosecPcap", "Use nanosecond timestamps in pcap trace", useNanosecPcap);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Enable pcap tracing as per command line
    if (enableTracing) {
        p2p.EnablePcapAll("tcp-pcap-nanosec-example", false, useNanosecPcap);
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;

    // Install PacketSink on node 1
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install BulkSendApplication on node 0
    BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}