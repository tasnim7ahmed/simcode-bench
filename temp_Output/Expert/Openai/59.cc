#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint64_t maxBytes = 0;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("maxBytes", "Maximum bytes to send (0 = unlimited)", maxBytes);
    cmd.AddValue("tracing", "Enable tracing", tracing);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    if (tracing) {
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-bulk-send.tr"));
        p2p.EnablePcapAll("tcp-bulk-send", false);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));
    std::cout << "Total Bytes Received: " << sinkPtr->GetTotalRx() << std::endl;

    Simulator::Destroy();
    return 0;
}