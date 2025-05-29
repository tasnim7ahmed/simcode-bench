#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/queue-disc.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

void
TraceCwnd(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
    static std::ofstream cwndStream("cwnd-trace.txt", std::ios_base::app);
    double now = Simulator::Now().GetSeconds();
    cwndStream << now << " " << newCwnd << std::endl;
}

void
TraceRtt(std::string context, Time oldRtt, Time newRtt)
{
    static std::ofstream rttStream("rtt-trace.txt", std::ios_base::app);
    double now = Simulator::Now().GetSeconds();
    rttStream << now << " " << newRtt.GetMilliSeconds() << std::endl;
}

void
TraceQueueDiscPackets(std::string context, uint32_t oldVal, uint32_t newVal)
{
    static std::ofstream qlStream("queue-length.txt", std::ios_base::app);
    double now = Simulator::Now().GetSeconds();
    qlStream << now << " " << newVal << std::endl;
}

void
TraceQueueDiscDrops(std::string context, uint32_t oldVal, uint32_t newVal)
{
    static std::ofstream dropStream("queue-drops.txt", std::ios_base::app);
    double now = Simulator::Now().GetSeconds();
    dropStream << now << " " << newVal << std::endl;
}

void
TraceQueueDiscMarks(std::string context, uint32_t oldVal, uint32_t newVal)
{
    static std::ofstream markStream("queue-marks.txt", std::ios_base::app);
    double now = Simulator::Now().GetSeconds();
    markStream << now << " " << newVal << std::endl;
}

void
ThroughputTracer(Ptr<Application> sinkApp)
{
    static std::ofstream tpStream("throughput.txt", std::ios_base::app);
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp);
    static uint64_t lastTotalRx = 0;
    double now = Simulator::Now().GetSeconds();
    uint64_t curTotalRx = sink->GetTotalRx();
    double throughput = (curTotalRx - lastTotalRx) * 8.0 / 1e6;
    tpStream << now << " " << throughput << std::endl;
    lastTotalRx = curTotalRx;
    Simulator::Schedule(Seconds(1.0), &ThroughputTracer, sinkApp);
}

int
main(int argc, char* argv[])
{
    bool enablePcap = false;
    bool enableDctcp = false;
    uint32_t nClients = 2;
    uint32_t nServers = 2;
    uint32_t maxBytes = 10 * 1024 * 1024;
    double simTime = 20.0;
    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableDctcp", "Enable DCTCP for TCP flows", enableDctcp);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer clients, routers, servers;
    clients.Create(nClients);
    servers.Create(nServers);
    routers.Create(2);

    // CSMA/PointToPoint topology: clients--r0--r1--servers
    PointToPointHelper p2p0, p2p1;
    p2p0.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p0.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));

    // Bottleneck link between routers
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    InternetStackHelper stack;
    TrafficControlHelper tch;

    // Install on all nodes (incl. traffic-control layer)
    stack.InstallAll();

    // Establish links
    NetDeviceContainer devCliR0, devR0R1, devR1Srv;
    for (uint32_t i = 0; i < nClients; ++i)
    {
        NodeContainer pair(clients.Get(i), routers.Get(0));
        NetDeviceContainer dev = p2p0.Install(pair);
        devCliR0.Add(dev.Get(0));
        devCliR0.Add(dev.Get(1));
    }
    NetDeviceContainer tmp = bottleneck.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    devR0R1 = tmp;
    for (uint32_t i = 0; i < nServers; ++i)
    {
        NodeContainer pair(servers.Get(i), routers.Get(1));
        NetDeviceContainer dev = p2p1.Install(pair);
        devR1Srv.Add(dev.Get(0));
        devR1Srv.Add(dev.Get(1));
    }

    // Install Traffic Control w/ FqCoDel and dynamic queue limits on bottleneck
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    tch.SetQueueLimits("ns3::DynamicQueueLimits", "HoldTime", StringValue("100ms"));
    QueueDiscContainer qd = tch.Install(devR0R1.Get(0));

    // Assign IP addresses
    Ipv4AddressHelper addrs;
    std::vector<Ipv4InterfaceContainer> interfacesClients, interfacesServers;

    // Client <-> R0
    for (uint32_t i = 0; i < nClients; ++i)
    {
        std::ostringstream net;
        net << "10.0." << i << ".0";
        addrs.SetBase(net.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interf = addrs.Assign(NetDeviceContainer(devCliR0.Get(2 * i), devCliR0.Get(2 * i + 1)));
        interfacesClients.push_back(interf);
    }

    // R0 <-> R1 (bottleneck)
    addrs.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer intfR0R1 = addrs.Assign(devR0R1);

    // Server <-> R1
    for (uint32_t i = 0; i < nServers; ++i)
    {
        std::ostringstream net;
        net << "10.2." << i << ".0";
        addrs.SetBase(net.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interf = addrs.Assign(NetDeviceContainer(devR1Srv.Get(2 * i + 1), devR1Srv.Get(2 * i)));
        interfacesServers.push_back(interf);
    }

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // DCTCP config if enabled
    if (enableDctcp)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpDctcp")));
    }
    else
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));
    }

    // ICMP/Ping application: Ping from each client to opposite server
    ApplicationContainer pingApps;
    for (uint32_t i = 0; i < nClients; ++i)
    {
        Ipv4Address dst = interfacesServers[i % nServers].GetAddress(0);
        PingHelper ping(dst);
        ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        ping.SetAttribute("Size", UintegerValue(56));
        ping.SetAttribute("Count", UintegerValue(5));
        ApplicationContainer app = ping.Install(clients.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime - 1));
        pingApps.Add(app);
    }

    // TCP BulkSend + PacketSink (src: client, dst: server)
    uint16_t port = 5000;
    ApplicationContainer bulkSendApps, sinkApps;
    for (uint32_t i = 0; i < nClients; ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(servers.Get(i % nServers));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));

        Ipv4Address sinkAddr = interfacesServers[i % nServers].GetAddress(0);
        BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(sinkAddr, port + i));
        bulk.SetAttribute("MaxBytes", UintegerValue(maxBytes));
        ApplicationContainer bulkApp = bulk.Install(clients.Get(i));
        bulkApp.Start(Seconds(2.0));
        bulkApp.Stop(Seconds(simTime));

        bulkSendApps.Add(bulkApp);
        sinkApps.Add(sinkApp);
    }

    // Install queue disc tracers (packet drops, marks, queue length) on bottleneck
    Ptr<QueueDisc> bottleneckDisc = qd.Get(0);
    bottleneckDisc->TraceConnect("PacketsInQueue", "", MakeCallback(&TraceQueueDiscPackets));
    bottleneckDisc->TraceConnect("DroppedPackets", "", MakeCallback(&TraceQueueDiscDrops));
    bottleneckDisc->TraceConnect("MarkedPackets", "", MakeCallback(&TraceQueueDiscMarks));

    // Setup TCP CWND and RTT traces
    for (uint32_t i = 0; i < nClients; ++i)
    {
        Ptr<Node> client = clients.Get(i);
        client->GetDevice(0)->GetNode()->GetObject<TcpSocketFactory>()->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TraceCwnd));
        client->GetDevice(0)->GetNode()->GetObject<TcpSocketFactory>()->TraceConnectWithoutContext("RTT", MakeCallback(&TraceRtt));
    }

    // Throughput trace
    for (uint32_t i = 0; i < sinkApps.GetN(); ++i)
    {
        Simulator::Schedule(Seconds(2.1), &ThroughputTracer, sinkApps.Get(i));
    }

    // Additional DCTCP traces if enabled
    if (enableDctcp)
    {
        Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/Alpha", MakeCallback([](std::string context, double oldVal, double newVal)
        {
            static std::ofstream dctcpStream("dctcp-alpha.txt", std::ios_base::app);
            dctcpStream << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
        }));
        Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/Ce", MakeCallback([](std::string context, uint32_t oldVal, uint32_t newVal)
        {
            static std::ofstream ceStream("dctcp-ce.txt", std::ios_base::app);
            ceStream << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
        }));
    }

    // Enable PCAP tracing if requested
    if (enablePcap)
    {
        bottleneck.EnablePcapAll("bottleneck");
        p2p0.EnablePcapAll("client-router0");
        p2p1.EnablePcapAll("router1-server");
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}