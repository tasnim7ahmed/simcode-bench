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

std::map<Ptr<Socket>, uint32_t> cWndMap;
std::map<Ptr<Socket>, uint32_t> ssThreshMap;
std::map<Ptr<Socket>, Time> rttMap;
std::map<Ptr<Socket>, Time> rtoMap;
std::map<Ptr<Socket>, uint32_t> inFlightMap;

std::ofstream cWndFile("cwnd.dat");
std::ofstream ssThreshFile("ssthresh.dat");
std::ofstream rttFile("rtt.dat");
std::ofstream rtoFile("rto.dat");
std::ofstream inFlightFile("inflight.dat");

void
CwndTracer(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
{
    cWndMap[socket] = newVal;
    cWndFile << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void
SsThreshTracer(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
{
    ssThreshMap[socket] = newVal;
    ssThreshFile << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void
RttTracer(Ptr<Socket> socket, Time oldVal, Time newVal)
{
    rttMap[socket] = newVal;
    rttFile << Simulator::Now().GetSeconds() << " " << newVal.GetSeconds() << std::endl;
}

void
RtoTracer(Ptr<Socket> socket, Time oldVal, Time newVal)
{
    rtoMap[socket] = newVal;
    rtoFile << Simulator::Now().GetSeconds() << " " << newVal.GetSeconds() << std::endl;
}

void
InFlightTracer(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
{
    inFlightMap[socket] = newVal;
    inFlightFile << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void
SetupTcpSocketMonitor(Ptr<Socket> socket)
{
    socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
    socket->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&SsThreshTracer));
    socket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTracer));
    socket->TraceConnectWithoutContext("RTO", MakeCallback(&RtoTracer));
    socket->TraceConnectWithoutContext("BytesInFlight", MakeCallback(&InFlightTracer));
}

void
CreateApplications(NodeContainer nodes, uint16_t port, DataRate bandwidth, uint32_t packetSize, double interval)
{
    // Server application
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client application
    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port));
    client.SetAttribute("DataRate", DataRateValue(bandwidth));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(10.0));
}

int
main(int argc, char *argv[])
{
    std::string tcpVariant = "TcpWestwood";
    std::string linkBandwidth = "1Mbps";
    std::string linkDelay = "20ms";
    double packetErrorRate = 0.0;
    uint32_t packetSize = 512;
    double interval = 0.001; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP variant to use: TcpWestwood or TcpWestwoodPlus", tcpVariant);
    cmd.AddValue("bandwidth", "Link bandwidth", linkBandwidth);
    cmd.AddValue("delay", "Link delay", linkDelay);
    cmd.AddValue("errorRate", "Packet error rate", packetErrorRate);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("interval", "Interval between packets (seconds)", interval);
    cmd.Parse(argc, argv);

    if (tcpVariant == "TcpWestwood")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
    }
    else if (tcpVariant == "TcpWestwoodPlus")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwoodPlus::GetTypeId()));
    }

    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", DataRateValue(DataRate(linkBandwidth)));
    Config::SetDefault("ns3::PointToPointChannel::Delay", TimeValue(Time(linkDelay)));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("100p")));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    CreateApplications(nodes, port, DataRate(linkBandwidth), packetSize, interval);

    if (packetErrorRate > 0.0)
    {
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        for (auto devIt = devices.Begin(); devIt != devices.End(); ++devIt)
        {
            Ptr<PointToPointNetDevice> p2pdev = DynamicCast<PointToPointNetDevice>(*devIt);
            if (p2pdev)
            {
                p2pdev->SetAttribute("ReceiveErrorModel", PointerValue(CreateObject<RateErrorModel>()));
                p2pdev->GetAttribute("ReceiveErrorModel", PointerValue());
                PointerValue ptr;
                p2pdev->GetAttribute("ReceiveErrorModel", ptr);
                Ptr<ErrorModel> em = ptr.Get<ErrorModel>();
                em->SetAttribute("ErrorRate", DoubleValue(packetErrorRate));
                em->SetAttribute("RanVar", PointerValue(uv));
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback(&CwndTracer));
    Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/SlowStartThreshold", MakeCallback(&SsThreshTracer));
    Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTT", MakeCallback(&RttTracer));
    Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTO", MakeCallback(&RtoTracer));
    Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/BytesInFlight", MakeCallback(&InFlightTracer));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    cWndFile.close();
    ssThreshFile.close();
    rttFile.close();
    rtoFile.close();
    inFlightFile.close();

    return 0;
}