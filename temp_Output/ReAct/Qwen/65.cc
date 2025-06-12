#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimulationScript");

int main(int argc, char *argv[]) {
    bool enablePcap = true;
    bool enableDctcp = false;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("dctcp", "Enable DCTCP tracing", enableDctcp);
    cmd.AddValue("time", "Simulation duration in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(2));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc",
                         "MaxFlows", UintegerValue(1024),
                         "FlowTimeout", TimeValue(Seconds(10)));
    tch.Install(devices[2]);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[3];

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(devices[1]);
    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(devices[2]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    PingHelper ping(interfaces[2].GetAddress(1));
    ping.SetRemote(interfaces[2].GetAddress(1));
    ping.Install(nodes.Get(0)).Start(Seconds(0.0));
    ping.Install(nodes.Get(1)).Start(Seconds(0.0));

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < 2; ++i) {
        sinkApps.Add(packetSinkHelper.Install(nodes.Get(3)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    BulkSendHelper sourceAppHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces[2].GetAddress(1), port));
    sourceAppHelper.SetAttribute("MaxBytes", UintegerValue(10000000));
    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < 2; ++i) {
        sourceApps.Add(sourceAppHelper.Install(nodes.Get(i)));
    }
    sourceApps.Start(Seconds(0.5));
    sourceApps.Stop(Seconds(simulationTime - 0.1));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("queue-disc-trace.tr");
    QueueDisc::GetQueueDiscsFromNetDevices(devices[2])->Get(0)->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&QueueDisc::EnqueueTrace, stream));
    QueueDisc::GetQueueDiscsFromNetDevices(devices[2])->Get(0)->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(&QueueDisc::DequeueTrace, stream));
    QueueDisc::GetQueueDiscsFromNetDevices(devices[2])->Get(0)->TraceConnectWithoutContext("Drop", MakeBoundCallback(&QueueDisc::DropTrace, stream));

    if (enablePcap) {
        p2p.EnablePcapAll("simulation-pcap");
    }

    if (enableDctcp) {
        Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback([](std::string path, const Time& now, const Time& delta, uint32_t oldCwnd, uint32_t newCwnd) {
            std::ofstream cwndFile("cwnd-trace.dat", std::ios_base::app);
            cwndFile << now.GetSeconds() << " " << newCwnd << std::endl;
        }));
    }

    Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/RTT", MakeCallback([](std::string path, const Time& now, const Time& rtt) {
        std::ofstream rttFile("rtt-trace.dat", std::ios_base::app);
        rttFile << now.GetSeconds() << " " << rtt.GetSeconds() << std::endl;
    }));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->Start(Seconds(0.5));
    monitor->Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}