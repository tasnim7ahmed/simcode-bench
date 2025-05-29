#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include <iostream>
#include <fstream>
#include <map>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWestwoodPlusSimulation");

// Maps to hold output file streams per flow
std::map<uint32_t, std::ofstream*> cwndStreams;
std::map<uint32_t, std::ofstream*> ssthreshStreams;
std::map<uint32_t, std::ofstream*> rttStreams;
std::map<uint32_t, std::ofstream*> rtoStreams;
std::map<uint32_t, std::ofstream*> inflightStreams;

// Tracing functions
void
CwndTracer(uint32_t oldval, uint32_t newval, uint32_t nodeId, uint32_t socketId)
{
    std::ofstream* out = cwndStreams[socketId];
    if (out)
        (*out) << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

void
SsthreshTracer(uint32_t oldval, uint32_t newval, uint32_t nodeId, uint32_t socketId)
{
    std::ofstream* out = ssthreshStreams[socketId];
    if (out)
        (*out) << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

void
RttTracer(Time oldval, Time newval, uint32_t nodeId, uint32_t socketId)
{
    std::ofstream* out = rttStreams[socketId];
    if (out)
        (*out) << Simulator::Now().GetSeconds() << " " << newval.GetMilliSeconds() << std::endl;
}

void
RtoTracer(Time oldval, Time newval, uint32_t nodeId, uint32_t socketId)
{
    std::ofstream* out = rtoStreams[socketId];
    if (out)
        (*out) << Simulator::Now().GetSeconds() << " " << newval.GetMilliSeconds() << std::endl;
}

void
InflightTracer(uint32_t oldval, uint32_t newval, uint32_t nodeId, uint32_t socketId)
{
    std::ofstream* out = inflightStreams[socketId];
    if (out)
        (*out) << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

void
SetupTcpTracing(Ptr<Socket> socket, uint32_t nodeId, uint32_t socketId)
{
    std::ostringstream ossCwnd, ossSsthresh, ossRtt, ossRto, ossInflight;
    ossCwnd << "cwnd-node" << nodeId << "-sock" << socketId << ".dat";
    ossSsthresh << "ssthresh-node" << nodeId << "-sock" << socketId << ".dat";
    ossRtt << "rtt-node" << nodeId << "-sock" << socketId << ".dat";
    ossRto << "rto-node" << nodeId << "-sock" << socketId << ".dat";
    ossInflight << "inflight-node" << nodeId << "-sock" << socketId << ".dat";

    auto *cwndFile = new std::ofstream(ossCwnd.str(), std::ios::out);
    auto *ssthreshFile = new std::ofstream(ossSsthresh.str(), std::ios::out);
    auto *rttFile = new std::ofstream(ossRtt.str(), std::ios::out);
    auto *rtoFile = new std::ofstream(ossRto.str(), std::ios::out);
    auto *inflightFile = new std::ofstream(ossInflight.str(), std::ios::out);

    cwndStreams[socketId] = cwndFile;
    ssthreshStreams[socketId] = ssthreshFile;
    rttStreams[socketId] = rttFile;
    rtoStreams[socketId] = rtoFile;
    inflightStreams[socketId] = inflightFile;

    socket->TraceConnectWithoutContext(
        "CongestionWindow", MakeBoundCallback(&CwndTracer, nodeId, socketId));

    socket->TraceConnectWithoutContext(
        "SlowStartThreshold", MakeBoundCallback(&SsthreshTracer, nodeId, socketId));

    socket->TraceConnectWithoutContext(
        "RTT", MakeBoundCallback(&RttTracer, nodeId, socketId));

    socket->TraceConnectWithoutContext(
        "RTO", MakeBoundCallback(&RtoTracer, nodeId, socketId));

    socket->TraceConnectWithoutContext(
        "BytesInFlight", MakeBoundCallback(&InflightTracer, nodeId, socketId));
}

void
InstallBulkSendApp(
    Ptr<Node> senderNode,
    Ipv4Address dstAddr,
    uint16_t port,
    uint32_t maxBytes,
    double startTime,
    double stopTime,
    uint32_t flowId)
{
    BulkSendHelper helper("ns3::TcpSocketFactory", InetSocketAddress(dstAddr, port));
    helper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer app = helper.Install(senderNode);
    app.Start(Seconds(startTime));
    app.Stop(Seconds(stopTime));
    Ptr<BulkSendApplication> appPtr = DynamicCast<BulkSendApplication>(app.Get(0));

    Ptr<Socket> sock = appPtr->GetSocket();
    if (sock != nullptr)
    {
        SetupTcpTracing(sock, senderNode->GetId(), flowId);
    }
}

// Command line configuration structure
struct SimParams
{
    std::string bandwidth = "10Mbps";
    std::string delay = "20ms";
    double errorRate = 0.0;
    std::string tcpVariant = "TcpWestwoodPlus";
    uint32_t flows = 1;
    uint32_t flowBytes = 0;
    double simDuration = 10.0;
};

int
main(int argc, char* argv[])
{
    SimParams params;
    CommandLine cmd;
    cmd.AddValue("bandwidth", "Data rate for point-to-point links", params.bandwidth);
    cmd.AddValue("delay", "Propagation delay for point-to-point links", params.delay);
    cmd.AddValue("errorRate", "Packet error rate on the bottle neck", params.errorRate);
    cmd.AddValue("tcpVariant", "TCP variant (TcpWestwood, TcpWestwoodPlus, etc.)", params.tcpVariant);
    cmd.AddValue("flows", "Number of parallel flows", params.flows);
    cmd.AddValue("flowBytes", "MaxBytes per flow (0=unlimited)", params.flowBytes);
    cmd.AddValue("simDuration", "Simulation duration (sec)", params.simDuration);
    cmd.Parse(argc, argv);

    // Set the selected TCP variant
    std::string tcpTypeId = "ns3::" + params.tcpVariant;
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpTypeId));

    // Topology: n0 ---- n1 ---- n2
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper ptp;
    ptp.SetDeviceAttribute("DataRate", StringValue(params.bandwidth));
    ptp.SetChannelAttribute("Delay", StringValue(params.delay));

    NetDeviceContainer d01 = ptp.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = ptp.Install(nodes.Get(1), nodes.Get(2));

    // Add error model on the bottleneck link
    if (params.errorRate > 0.0)
    {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(params.errorRate));
        d12.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = ipv4.Assign(d01);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = ipv4.Assign(d12);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install sink apps on n2
    uint16_t basePort = 5000;
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < params.flows; ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), basePort + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer localSink = sinkHelper.Install(nodes.Get(2));
        localSink.Start(Seconds(0.0));
        localSink.Stop(Seconds(params.simDuration));
        sinkApps.Add(localSink);
    }

    // Install sources (bulk send) on n0
    for (uint32_t i = 0; i < params.flows; ++i)
    {
        InstallBulkSendApp(
            nodes.Get(0),
            if12.GetAddress(1),
            basePort + i,
            params.flowBytes,
            0.1 + i * 0.01,
            params.simDuration,
            i
        );
    }

    Simulator::Stop(Seconds(params.simDuration));
    Simulator::Run();
    Simulator::Destroy();

    // Clean up file streams
    for (auto &kv : cwndStreams) delete kv.second;
    for (auto &kv : ssthreshStreams) delete kv.second;
    for (auto &kv : rttStreams) delete kv.second;
    for (auto &kv : rtoStreams) delete kv.second;
    for (auto &kv : inflightStreams) delete kv.second;

    return 0;
}