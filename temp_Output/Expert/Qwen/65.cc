#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimulationScript");

int main(int argc, char *argv[]) {
    uint16_t port = 9;
    std::string transportProt = "Tcp";
    bool enableDctcp = false;
    bool pcapTracing = true;
    Time stopTime = Seconds(10.0);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc",
                         "EnableCeThreshold", BooleanValue(true),
                         "CeThreshold", TimeValue(MilliSeconds(5)),
                         "UseL4s", BooleanValue(enableDctcp));
    tch.SetQueue("ns3::DynamicQueueLimits");

    NetDeviceContainer devices = p2p.Install(nodes.Get(0), nodes.Get(1));

    InternetStackHelper stack;
    stack.SetTcpDefaultCongestionControl("ns3::TcpNewReno");
    if (enableDctcp) {
        Config::SetDefault("ns3::TcpSocketBase::UseEcn", EnumValue(TcpSocketBase::On));
        Config::SetDefault("ns3::TcpSocketBase::EcnMode", EnumValue(TcpSocketBase::DctcpEcn));
    }
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    PingHelper ping(interfaces.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    ping.SetAttribute("Size", UintegerValue(512));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(0.0));
    pingApp.Stop(stopTime);

    BulkSendHelper source("ns3::" + transportProt + SocketFactory::GetTypeId(), InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(stopTime);

    PacketSinkHelper sink("ns3::" + transportProt + SocketFactory::GetTypeId(), InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(stopTime);

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("queue-disc.tr");
    QueueDisc::EnableQueueDiscPacketsTrace(stream);
    QueueDisc::EnableQueueDiscBytesTrace(stream);
    QueueDisc::EnableQueueDiscDropTrace(stream);
    QueueDisc::EnableQueueDiscMarkTrace(stream);

    if (pcapTracing) {
        p2p.EnablePcapAll("simulation");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Schedule(stopTime, &FlowMonitor::SerializeToXmlStream, monitor, std::cout, 0, 0);

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Cwnd", MakeCallback([](std::string path, uint32_t oldVal, uint32_t newVal) {
        NS_LOG_INFO("CWND changed at " << Simulator::Now().As(Time::S) << " from " << oldVal << " to " << newVal);
    }));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Rtt", MakeCallback([](std::string path, Time oldVal, Time newVal) {
        NS_LOG_INFO("RTT changed at " << Simulator::Now().As(Time::S) << " from " << oldVal.As(Time::MS) << " to " << newVal.As(Time::MS));
    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/BytesInQueue", MakeCallback([](std::string path, uint32_t oldVal, uint32_t newVal) {
        NS_LOG_INFO("TX Queue Bytes: " << newVal << " at " << Simulator::Now().As(Time::S));
    }));

    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}