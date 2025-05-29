#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint64_t maxBytes = 0;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("maxBytes", "Total number of bytes to send (0=unlimited)", maxBytes);
    cmd.AddValue("tracing", "Enable trace files", tracing);
    cmd.Parse(argc, argv);

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

    // Install BulkSendApplication on n0
    uint16_t port = 9;
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                             InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Install PacketSinkApplication on n1
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    if (tracing) {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-bulk-send.tr"));
        pointToPoint.EnablePcapAll("tcp-bulk-send", false);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    std::cout << "Total Bytes Received: " << sink->GetTotalRx() << std::endl;

    Simulator::Destroy();

    return 0;
}