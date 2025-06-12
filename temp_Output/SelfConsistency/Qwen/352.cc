#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpP2PSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEcho applications
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Creating channel.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Ensure server is at 192.168.1.2
    Ptr<Ipv4> ipv4Server = nodes.Get(1)->GetObject<Ipv4>();
    uint32_t interfaceIndexServer = 0;
    ipv4Server->SetIpForward(interfaceIndexServer, Ipv4Address("192.168.1.2"));

    NS_LOG_INFO("Setting up TCP server.");
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    NS_LOG_INFO("Setting up TCP client.");
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("192.168.1.2"), 9));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(11.0));

    NS_LOG_INFO("Enabling flow monitor.");
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed.");
    return 0;
}