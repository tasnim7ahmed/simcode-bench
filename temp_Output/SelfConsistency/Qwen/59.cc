#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkSendExample");

int
main(int argc, char* argv[])
{
    uint64_t maxBytes = 0;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("maxBytes", "Total number of bytes to send (0 for unlimited)", maxBytes);
    cmd.AddValue("tracing", "Enable ASCII and pcap tracing", tracing);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Creating channel.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Creating applications.");
    uint16_t sinkPort = 8080;

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper senderHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    senderHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer senderApp = senderHelper.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    if (tracing)
    {
        AsciiTraceHelper asciiTraceHelper;
        pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-bulk-send.tr"));
        pointToPoint.EnablePcapAll("tcp-bulk-send");
    }

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    if (sink)
    {
        std::cout << "Total Bytes Received: " << sink->GetTotalRx() << " bytes" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}