#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <fstream>
#include <iostream>
#include <map>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

std::ofstream CongestionWindowTrace;
std::ofstream SlowStartThresholdTrace;
std::ofstream RttTrace;
std::ofstream RtoTrace;
std::ofstream InFlightBytesTrace;

std::map<Ptr<TcpSocket>, Ipv4Address> SocketToAddress;

void CongestionWindowTracer(std::string context, uint32_t oldCwnd, uint32_t newCwnd) {
    std::string socketAddress = SocketToAddress[Simulator::GetObject<TcpSocket>(context.substr(0, context.find("/")))].MakeU8string();
    CongestionWindowTrace << Simulator::Now().GetSeconds() << " " << socketAddress << " " << newCwnd << std::endl;
}

void SlowStartThresholdTracer(std::string context, uint32_t oldSsthresh, uint32_t newSsthresh) {
    std::string socketAddress = SocketToAddress[Simulator::GetObject<TcpSocket>(context.substr(0, context.find("/")))].MakeU8string();
    SlowStartThresholdTrace << Simulator::Now().GetSeconds() << " " << socketAddress << " " << newSsthresh << std::endl;
}

void RttTracer(std::string context, Time oldRtt, Time newRtt) {
    std::string socketAddress = SocketToAddress[Simulator::GetObject<TcpSocket>(context.substr(0, context.find("/")))].MakeU8string();
    RttTrace << Simulator::Now().GetSeconds() << " " << socketAddress << " " << newRtt.GetSeconds() << std::endl;
}

void RtoTracer(std::string context, Time oldRto, Time newRto) {
    std::string socketAddress = SocketToAddress[Simulator::GetObject<TcpSocket>(context.substr(0, context.find("/")))].MakeU8string();
    RtoTrace << Simulator::Now().GetSeconds() << " " << socketAddress << " " << newRto.GetSeconds() << std::endl;
}

void InFlightBytesTracer(std::string context, uint32_t oldInFlightBytes, uint32_t newInFlightBytes) {
    std::string socketAddress = SocketToAddress[Simulator::GetObject<TcpSocket>(context.substr(0, context.find("/")))].MakeU8string();
    InFlightBytesTrace << Simulator::Now().GetSeconds() << " " << socketAddress << " " << newInFlightBytes << std::endl;
}

int main(int argc, char *argv[]) {
    CommandLine cmd;

    std::string bandwidth = "10Mbps";
    std::string delay = "10ms";
    double errorRate = 0.0;
    std::string tcpVariant = "TcpWestwoodPlus";
    int numFlows = 1;

    cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
    cmd.AddValue("delay", "Bottleneck delay", delay);
    cmd.AddValue("errorRate", "Bottleneck error rate", errorRate);
    cmd.AddValue("tcpVariant", "TCP variant", tcpVariant);
    cmd.AddValue("numFlows", "Number of flows", numFlows);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpVariant));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));
    pointToPoint.SetQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    ApplicationContainer sinkApps;
    ApplicationContainer clientApps;

    for (int i = 0; i < numFlows; ++i) {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinkApps.Add(sinkHelper.Install(nodes.Get(1)));
        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(10.0));

        Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
        Ptr<TcpSocket> tcpSocket = DynamicCast<TcpSocket>(ns3TcpSocket);
        SocketToAddress[tcpSocket] = interfaces.GetAddress(1);

        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port + i));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        clientApps.Add(source.Install(nodes.Get(0)));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    CongestionWindowTrace.open("cwnd.dat");
    SlowStartThresholdTrace.open("ssthresh.dat");
    RttTrace.open("rtt.dat");
    RtoTrace.open("rto.dat");
    InFlightBytesTrace.open("inflight.dat");

    for (uint32_t i = 0; i < clientApps.GetNApplications(); ++i) {
        Ptr<BulkSendApplication> app = DynamicCast<BulkSendApplication>(clientApps.Get(i));
        Ptr<Socket> socket = app->GetSocket();
        std::string context = "/NodeList/*/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socket->GetId());

        Config::ConnectWithoutContext(context + "/CongestionWindow", MakeCallback(&CongestionWindowTracer));
        Config::ConnectWithoutContext(context + "/SlowStartThreshold", MakeCallback(&SlowStartThresholdTracer));
        Config::ConnectWithoutContext(context + "/RTT", MakeCallback(&RttTracer));
        Config::ConnectWithoutContext(context + "/RTO", MakeCallback(&RtoTracer));
        Config::ConnectWithoutContext(context + "/BytesInFlight", MakeCallback(&InFlightBytesTracer));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

    monitor->SerializeToXmlFile("tcp-westwood-plus.flowmon", true, true);

    CongestionWindowTrace.close();
    SlowStartThresholdTrace.close();
    RttTrace.close();
    RtoTrace.close();
    InFlightBytesTrace.close();

    Simulator::Destroy();
    return 0;
}