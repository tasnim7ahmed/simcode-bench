#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/stats-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

std::ofstream CongestionWindowTraceStream;
std::ofstream SlowStartThresholdTraceStream;
std::ofstream RttTraceStream;
std::ofstream RtoTraceStream;
std::ofstream InFlightBytesTraceStream;

std::map<Ptr<TcpSocket>, int> SocketFlowMap;

void CwndTracer(std::string key, Time oldval, Time newval) {
    Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(Simulator::GetObject(key));
    int flowId = SocketFlowMap[socket];
    CongestionWindowTraceStream << Simulator::Now().GetSeconds() << " " << flowId << " " << newval.GetMicroSeconds() << std::endl;
}

void SsThreshTracer(std::string key, uint32_t oldval, uint32_t newval) {
    Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(Simulator::GetObject(key));
    int flowId = SocketFlowMap[socket];
    SlowStartThresholdTraceStream << Simulator::Now().GetSeconds() << " " << flowId << " " << newval << std::endl;
}

void RttTracer(std::string key, Time oldval, Time newval) {
    Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(Simulator::GetObject(key));
    int flowId = SocketFlowMap[socket];
    RttTraceStream << Simulator::Now().GetSeconds() << " " << flowId << " " << newval.GetSeconds() << std::endl;
}

void RtoTracer(std::string key, Time oldval, Time newval) {
    Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(Simulator::GetObject(key));
    int flowId = SocketFlowMap[socket];
    RtoTraceStream << Simulator::Now().GetSeconds() << " " << flowId << " " << newval.GetSeconds() << std::endl;
}

void InFlightBytesTracer(std::string key, uint32_t oldval, uint32_t newval) {
  Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(Simulator::GetObject(key));
  int flowId = SocketFlowMap[socket];
  InFlightBytesTraceStream << Simulator::Now().GetSeconds() << " " << flowId << " " << newval << std::endl;
}

int main(int argc, char* argv[]) {
    bool enablePcap = false;
    std::string tcpVariant = "TcpWestwoodPlus";
    std::string bandwidth = "10Mbps";
    std::string delay = "2ms";
    double errorRate = 0.00001;
    double simDuration = 10;
    uint32_t packetSize = 1024;
    uint32_t numFlows = 1;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpTahoe, TcpReno, TcpNewReno, TcpWestwood, TcpWestwoodPlus", tcpVariant);
    cmd.AddValue("bandwidth", "Bottleneck link bandwidth", bandwidth);
    cmd.AddValue("delay", "Bottleneck link delay", delay);
    cmd.AddValue("errorRate", "Bottleneck link error rate", errorRate);
    cmd.AddValue("simDuration", "Simulation duration", simDuration);
    cmd.AddValue("packetSize", "Packet size", packetSize);
    cmd.AddValue("numFlows", "Number of flows", numFlows);
    cmd.Parse(argc, argv);

    LogComponentEnable("TcpWestwoodPlusSimulation", LOG_LEVEL_INFO);

    // Configure TCP variant
    if (tcpVariant == "TcpTahoe") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpTahoe"));
    } else if (tcpVariant == "TcpReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpReno"));
    } else if (tcpVariant == "TcpNewReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    } else if (tcpVariant == "TcpWestwood") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpWestwood"));
    } else if (tcpVariant == "TcpWestwoodPlus") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpWestwoodPlus"));
    } else {
        std::cerr << "Invalid TCP variant.  Supported values: TcpTahoe, TcpReno, TcpNewReno, TcpWestwood, TcpWestwoodPlus" << std::endl;
        return 1;
    }

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Configure error model
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    pointToPoint.SetErrorModel(em);

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup BulkSendApplication and PacketSinkApplication
    uint16_t port = 50000;
    ApplicationContainer sinkApp;
    ApplicationContainer sourceApp;

    for (uint32_t i = 0; i < numFlows; ++i) {
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port + i));
        sinkApp.Add(packetSinkHelper.Install(nodes.Get(1)));

        BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port + i));
        bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
        bulkSendHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        sourceApp.Add(bulkSendHelper.Install(nodes.Get(0)));
    }

    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(simDuration + 1));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(simDuration));

    // Tracing
    CongestionWindowTraceStream.open("cwnd.dat");
    SlowStartThresholdTraceStream.open("ssthresh.dat");
    RttTraceStream.open("rtt.dat");
    RtoTraceStream.open("rto.dat");
    InFlightBytesTraceStream.open("inflight.dat");

    // Populate the SocketFlowMap
    for (uint32_t i = 0; i < sourceApp.GetNApplications(); ++i) {
        Ptr<BulkSendApplication> source = DynamicCast<BulkSendApplication>(sourceApp.Get(i));
        Ptr<TcpSocket> socket = source->GetSocket();
        SocketFlowMap[socket] = i;
    }

    // Connect traces
    for (uint32_t i = 0; i < sourceApp.GetNApplications(); ++i) {
        Ptr<BulkSendApplication> source = DynamicCast<BulkSendApplication>(sourceApp.Get(i));
        Ptr<TcpSocket> socket = source->GetSocket();
        std::stringstream congestionWindowPath;
        congestionWindowPath << "/NodeList/" << nodes.Get(0)->GetId() << "/$ns3::TcpL4Protocol/SocketList/" << socket->GetId() << "/CongestionWindow";
        Config::Connect(congestionWindowPath.str(), MakeCallback(&CwndTracer));

        std::stringstream ssThreshPath;
        ssThreshPath << "/NodeList/" << nodes.Get(0)->GetId() << "/$ns3::TcpL4Protocol/SocketList/" << socket->GetId() << "/SlowStartThreshold";
        Config::Connect(ssThreshPath.str(), MakeCallback(&SsThreshTracer));

        std::stringstream rttPath;
        rttPath << "/NodeList/" << nodes.Get(0)->GetId() << "/$ns3::TcpL4Protocol/SocketList/" << socket->GetId() << "/RTT";
        Config::Connect(rttPath.str(), MakeCallback(&RttTracer));

        std::stringstream rtoPath;
        rtoPath << "/NodeList/" << nodes.Get(0)->GetId() << "/$ns3::TcpL4Protocol/SocketList/" << socket->GetId() << "/RTO";
        Config::Connect(rtoPath.str(), MakeCallback(&RtoTracer));

        std::stringstream inFlightPath;
        inFlightPath << "/NodeList/" << nodes.Get(0)->GetId() << "/$ns3::TcpL4Protocol/SocketList/" << socket->GetId() << "/BytesInFlight";
        Config::Connect(inFlightPath.str(), MakeCallback(&InFlightBytesTracer));

    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simDuration));

    if (enablePcap) {
        pointToPoint.EnablePcapAll("tcp_westwood_plus");
    }

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Run();

    flowMonitor->SerializeToXmlFile("tcp_westwood_plus_flowmon.xml", false, true);

    CongestionWindowTraceStream.close();
    SlowStartThresholdTraceStream.close();
    RttTraceStream.close();
    RtoTraceStream.close();
    InFlightBytesTraceStream.close();

    Simulator::Destroy();
    return 0;
}