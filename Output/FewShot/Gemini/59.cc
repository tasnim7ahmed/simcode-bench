#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/net-device.h"
#include "ns3/packet-sink.h"
#include "ns3/bulk-send-application.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool tracing = false;
    uint64_t maxBytes = 0;

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("maxBytes", "Maximum number of bytes to send", maxBytes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(2.0));
    sourceApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (tracing) {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll("tcp-bulk-send.tr");
        pointToPoint.EnablePcapAll("tcp-bulk-send");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    uint64_t totalBytesReceived = DynamicCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
    std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;

    Simulator::Destroy();
    return 0;
}