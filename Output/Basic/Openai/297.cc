#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    // Install TCP receiver (PacketSink) on Node 1
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP sender (BulkSendApplication) on Node 0
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data to send
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));

    // Set TCP NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    // FlowMonitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("tcp-newreno-throughput.xml", true, true);

    Simulator::Destroy();
    return 0;
}