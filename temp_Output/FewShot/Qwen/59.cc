#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint64_t maxBytes = 0;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("maxBytes", "Total bytes to send (default: 0 means unlimited)", maxBytes);
    cmd.AddValue("tracing", "Enable ascii and pcap tracing", tracing);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSinkApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    if (tracing) {
        AsciiTraceHelper asciiTraceHelper;
        pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-bulk-send.tr"));
        pointToPoint.EnablePcapAll("tcp-bulk-send");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<PacketSink> sinkAppPtr = DynamicCast<PacketSink>(sinkApp.Get(0));
    std::cout << "Total Bytes Received: " << sinkAppPtr->GetTotalRx() << " bytes" << std::endl;

    Simulator::Destroy();
    return 0;
}