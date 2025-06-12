#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkSendExample");

int main(int argc, char *argv[]) {
    // Disable flow monitor logging for simplicity
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));

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

    uint16_t sinkPort = 9;

    // Create TCP sink (receiver) application on node 1
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create BulkSend application on node 0
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // 0 means unlimited data

    ApplicationContainer senderApp = bulkSendHelper.Install(nodes.Get(0));
    senderApp.Start(Seconds(2.0));
    senderApp.Stop(Seconds(10.0));

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-bulk-send.tr"));
    pointToPoint.EnablePcapAll("tcp-bulk-send");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}