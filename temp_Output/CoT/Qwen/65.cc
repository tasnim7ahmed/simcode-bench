#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    uint16_t nClients = 2;
    uint16_t port = 9;
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    bool enablePcap = true;
    bool enableDctcp = false;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of client nodes", nClients);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableDctcp", "Enable DCTCP tracing", enableDctcp);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer clients;
    clients.Create(nClients);

    NodeContainer servers;
    servers.Create(1);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("10ms"));

    TrafficControlHelper tchBottleneck;
    tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc",
                                   "MaxSize", StringValue("1024p"),
                                   "UseDynamicQueueLimits", BooleanValue(true));
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(clients.Get(0), servers.Get(0));
    tchBotInstall(tchBottleneck, bottleneckDevices);

    PointToPointHelper clientLinks;
    clientLinks.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    clientLinks.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientDevices;
    for (uint32_t i = 1; i < nClients; ++i) {
        NetDeviceContainer nd = clientLinks.Install(clients.Get(i), servers.Get(0));
        clientDevices.Add(nd);
    }

    InternetStackHelper stack;
    if (enableDctcp) {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpDctcp::GetTypeId()));
    }
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);
    for (uint32_t i = 0; i < clientDevices.GetN(); ++i) {
        address.NewNetwork();
        Ipv4InterfaceContainer ifc = address.Assign(clientDevices.Get(i));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    PingHelper ping(servers.Get(0)->GetObject<Node>()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ApplicationContainer pingApps = ping.Install(clients);
    pingApps.Start(Seconds(0.0));
    pingApps.Stop(Seconds(simulationTime));

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(bottleneckInterfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = source.Install(clients.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(servers.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("queue-disc-traces.tr");
    QueueDisc::EnableQueueDiscTracingAll(stream);

    if (enablePcap) {
        bottleneckLink.EnablePcapAll("bottleneck-link");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->SetAttribute("StartTime", TimeValue(Seconds(0)));
    monitor->SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/MaxBytes", MakeCallback([](Ptr<const AttributeValue> oldval, Ptr<const AttributeValue> newval) {
        NS_LOG_INFO("MaxBytes changed from " << oldval->SerializeToString(FindLogTimeFormat()) << " to " << newval->SerializeToString(FindLogTimeFormat()));
    }));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flow-monitor.xml", true, true);

    Simulator::Destroy();
    return 0;
}