// tcp-westwood-simulation.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"

#include <map>
#include <fstream>
#include <string>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWestwoodSimulation");

std::map<uint32_t, std::ofstream> cwndStreams;
std::map<uint32_t, std::ofstream> ssthreshStreams;
std::map<uint32_t, std::ofstream> rttStreams;
std::map<uint32_t, std::ofstream> rtoStreams;
std::map<uint32_t, std::ofstream> inflightStreams;

void
CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
    uint32_t nodeId = Simulator::GetContext ();
    std::ofstream &out = cwndStreams[nodeId];
    out << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

void
SsThreshTracer (std::string context, uint32_t oldSsThresh, uint32_t newSsThresh)
{
    uint32_t nodeId = Simulator::GetContext ();
    std::ofstream &out = ssthreshStreams[nodeId];
    out << Simulator::Now ().GetSeconds () << "\t" << newSsThresh << std::endl;
}

void
RttTracer (std::string context, Time oldRtt, Time newRtt)
{
    uint32_t nodeId = Simulator::GetContext ();
    std::ofstream &out = rttStreams[nodeId];
    out << Simulator::Now ().GetSeconds () << "\t" << newRtt.GetMilliSeconds () << std::endl;
}

void
RtoTracer (std::string context, Time oldRto, Time newRto)
{
    uint32_t nodeId = Simulator::GetContext ();
    std::ofstream &out = rtoStreams[nodeId];
    out << Simulator::Now ().GetSeconds () << "\t" << newRto.GetMilliSeconds () << std::endl;
}

void
InflightTracer (std::string context, uint32_t oldInflight, uint32_t newInflight)
{
    uint32_t nodeId = Simulator::GetContext ();
    std::ofstream &out = inflightStreams[nodeId];
    out << Simulator::Now ().GetSeconds () << "\t" << newInflight << std::endl;
}

void
InstallTraces (Ptr<Socket> socket, uint32_t flowId)
{
    std::ostringstream cwndFile, ssthreshFile, rttFile, rtoFile, inflightFile;

    cwndFile << "tcp-cwnd-" << flowId << ".dat";
    ssthreshFile << "tcp-ssthresh-" << flowId << ".dat";
    rttFile << "tcp-rtt-" << flowId << ".dat";
    rtoFile << "tcp-rto-" << flowId << ".dat";
    inflightFile << "tcp-inflight-" << flowId << ".dat";

    cwndStreams[flowId].open (cwndFile.str ());
    ssthreshStreams[flowId].open (ssthreshFile.str ());
    rttStreams[flowId].open (rttFile.str ());
    rtoStreams[flowId].open (rtoFile.str ());
    inflightStreams[flowId].open (inflightFile.str ());

    socket->TraceConnect ("CongestionWindow", "", MakeBoundCallback (&CwndTracer));
    socket->TraceConnect ("SlowStartThreshold", "", MakeBoundCallback (&SsThreshTracer));
    socket->TraceConnect ("RTT", "", MakeBoundCallback (&RttTracer));
    socket->TraceConnect ("RTO", "", MakeBoundCallback (&RtoTracer));
    socket->TraceConnect ("BytesInFlight", "", MakeBoundCallback (&InflightTracer));
}

void
ConfigureTcpVariant (std::string variant)
{
    if (variant == "TcpWestwood" || variant == "TcpWestwoodPlus")
    {
        if (variant == "TcpWestwood")
        {
            Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
        }
        else
        {
            // TcpWestwoodPlus is a subclass, can be used as well
            Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwoodPlus::GetTypeId ()));
            Config::SetDefault ("ns3::TcpWestwoodPlus::FilterType", EnumValue (TcpWestwoodPlus::TUSTIN));
        }
    }
    else
    {
        NS_LOG_UNCOND ("Unknown TCP variant; defaulting to TcpWestwood");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
    }
}

void
ConfigureError (Ptr<PointToPointNetDevice> device, double per)
{
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (per));
    device->SetReceiveErrorModel (em);
}

void
SetupFlow (Ptr<Node> sender, Ptr<Node> receiver, uint16_t port, double startTime, double stopTime, uint32_t flowId, uint32_t packetSize, DataRate rate)
{
    // Sender: Bulk Send App
    Address sinkAddress (InetSocketAddress (receiver->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port));
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (sender, TcpSocketFactory::GetTypeId ());

    InstallTraces (ns3TcpSocket, flowId);

    Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication> ();
    app->SetAttribute ("Socket", PointerValue (ns3TcpSocket));
    app->SetAttribute ("Remote", AddressValue (sinkAddress));
    app->SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited data
    sender->AddApplication (app);
    app->SetStartTime (Seconds (startTime));
    app->SetStopTime (Seconds (stopTime));

    // Receiver: Packet Sink
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp = sinkHelper.Install (receiver);
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (stopTime));
}

int
main (int argc, char *argv[])
{
    // Default simulation parameters
    std::string bandwidth = "10Mbps";
    std::string delay = "20ms";
    double errorRate = 0.0;
    std::string tcpVariant = "TcpWestwoodPlus";
    uint32_t numFlows = 2;
    double simTime = 20.0;
    uint32_t packetSize = 1000;
    std::string dataRate = "1Mbps";

    CommandLine cmd;
    cmd.AddValue ("bandwidth", "Link Bandwidth", bandwidth);
    cmd.AddValue ("delay", "Link Delay", delay);
    cmd.AddValue ("errorRate", "Packet error rate", errorRate);
    cmd.AddValue ("tcpVariant", "TCP Variant (TcpWestwood or TcpWestwoodPlus)", tcpVariant);
    cmd.AddValue ("numFlows", "Number of parallel flows", numFlows);
    cmd.AddValue ("simTime", "Total simulation time (s)", simTime);
    cmd.AddValue ("packetSize", "Packet size (bytes)", packetSize);
    cmd.AddValue ("dataRate", "App data rate", dataRate);
    cmd.Parse (argc, argv);

    ConfigureTcpVariant (tcpVariant);

    NodeContainer nodes;
    nodes.Create (2);

    // Point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
    p2p.SetChannelAttribute ("Delay", StringValue (delay));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));
    NetDeviceContainer devices = p2p.Install (nodes);

    // Configure error model on receiver side
    if (errorRate > 0.0)
    {
        Ptr<PointToPointNetDevice> p2pDev = devices.Get (1)->GetObject<PointToPointNetDevice> ();
        ConfigureError (p2pDev, errorRate);
    }

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Parallel flows: one per source port (to same dest)
    uint16_t port = 50000;

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        SetupFlow (nodes.Get (0), nodes.Get (1), port + i,
                   1.0 + i * 0.05, simTime, i, packetSize, DataRate (dataRate));
    }

    // Enable FlowMonitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> monitor = flowMonitorHelper.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Write FlowMonitor stats (optional)
    monitor->CheckForLostPackets ();
    monitor->SerializeToXmlFile ("tcp-westwood-flowmon.xml", true, true);

    Simulator::Destroy ();

    // Close all trace files
    for (auto &kv : cwndStreams) { if (kv.second.is_open ()) kv.second.close (); }
    for (auto &kv : ssthreshStreams) { if (kv.second.is_open ()) kv.second.close (); }
    for (auto &kv : rttStreams) { if (kv.second.is_open ()) kv.second.close (); }
    for (auto &kv : rtoStreams) { if (kv.second.is_open ()) kv.second.close (); }
    for (auto &kv : inflightStreams) { if (kv.second.is_open ()) kv.second.close (); }

    return 0;
}