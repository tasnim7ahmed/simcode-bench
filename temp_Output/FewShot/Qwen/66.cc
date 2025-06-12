#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-westwood.h"
#include <fstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodSimulation");

std::ofstream cwndFile("cwnd-trace.txt");
std::ofstream ssThreshFile("ssthresh-trace.txt");
std::ofstream rttFile("rtt-trace.txt");
std::ofstream rtoFile("rto-trace.txt");
std::ofstream inFlightFile("inflight-trace.txt");

std::map<Ptr<Socket>, uint32_t> flowIdMap;
uint32_t nextFlowId = 0;

void
TraceCwnd(Ptr<Socket> localSocket, uint32_t oldVal, uint32_t newVal)
{
    Time now = Simulator::Now();
    uint32_t flowId = flowIdMap[localSocket];
    cwndFile << now.GetSeconds() << "\t" << flowId << "\t" << newVal << std::endl;
}

void
TraceSsThresh(Ptr<Socket> localSocket, uint32_t oldVal, uint32_t newVal)
{
    Time now = Simulator::Now();
    uint32_t flowId = flowIdMap[localSocket];
    ssThreshFile << now.GetSeconds() << "\t" << flowId << "\t" << newVal << std::endl;
}

void
TraceRtt(Ptr<Socket> localSocket, Time oldVal, Time newVal)
{
    Time now = Simulator::Now();
    uint32_t flowId = flowIdMap[localSocket];
    rttFile << now.GetSeconds() << "\t" << flowId << "\t" << newVal.GetSeconds() << std::endl;
}

void
TraceRto(Ptr<Socket> localSocket, Time oldVal, Time newVal)
{
    Time now = Simulator::Now();
    uint32_t flowId = flowIdMap[localSocket];
    rtoFile << now.GetSeconds() << "\t" << flowId << "\t" << newVal.GetSeconds() << std::endl;
}

void
TraceInFlight(Ptr<Socket> localSocket, uint32_t oldVal, uint32_t newVal)
{
    Time now = Simulator::Now();
    uint32_t flowId = flowIdMap[localSocket];
    inFlightFile << now.GetSeconds() << "\t" << flowId << "\t" << newVal << std::endl;
}

void
SetupTracing(Ptr<Socket> socket)
{
    uint32_t flowId = nextFlowId++;
    flowIdMap[socket] = flowId;

    socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TraceCwnd));
    socket->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&TraceSsThresh));
    socket->TraceConnectWithoutContext("RTT", MakeCallback(&TraceRtt));
    socket->TraceConnectWithoutContext("RTO", MakeCallback(&TraceRto));
    socket->TraceConnectWithoutContext("BytesInFlight", MakeCallback(&TraceInFlight));
}

int main(int argc, char *argv[])
{
    std::string tcpVariant = "TcpWestwood";
    std::string bandwidth = "5Mbps";
    std::string delay = "20ms";
    double errorRate = 0.001;
    uint32_t maxPackets = 100;
    uint32_t packetSize = 1024;
    Time simulationTime = Seconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP variant type (e.g., TcpWestwood)", tcpVariant);
    cmd.AddValue("bandwidth", "Channel bandwidth", bandwidth);
    cmd.AddValue("delay", "Propagation delay", delay);
    cmd.AddValue("errorRate", "Packet error rate", errorRate);
    cmd.AddValue("maxPackets", "Maximum number of packets to send", maxPackets);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.Parse(argc, argv);

    // Set the TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    if (errorRate > 0.0)
    {
        p2p.SetDeviceAttribute("ReceiveErrorModel",
                               PointerValue(CreateObject<RateErrorModel>()));
        Config::SetDefault("ns3::RateErrorModel::ErrorRate", DoubleValue(errorRate));
    }

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install applications
    uint16_t port = 5000;

    // Server application
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    // Client application
    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    client.SetAttribute("MaxBytes", UintegerValue(maxPackets * packetSize));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(simulationTime);

    // Set up tracing for each socket
    Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionControl", MakeCallback(&SetupTracing));

    // Run simulation
    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    // Close output files
    cwndFile.close();
    ssThreshFile.close();
    rttFile.close();
    rtoFile.close();
    inFlightFile.close();

    return 0;
}