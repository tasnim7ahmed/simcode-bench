#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-westwood.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodSimulation");

class TcpTracer {
public:
    TcpTracer();
    void TraceCongestionWindow(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal);
    void TraceSlowStartThreshold(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal);
    void TraceRtt(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal);
    void TraceRto(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal);
    void TraceInFlight(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal);

private:
    std::map<std::string, uint32_t> m_cwndMap;
    std::map<std::string, uint32_t> m_ssthreshMap;
    std::map<std::string, Time> m_rttMap;
    std::map<std::string, Time> m_rtoMap;
    std::map<std::string, uint32_t> m_inFlightMap;
};

TcpTracer::TcpTracer() {}

void TcpTracer::TraceCongestionWindow(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void TcpTracer::TraceSlowStartThreshold(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void TcpTracer::TraceRtt(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
}

void TcpTracer::TraceRto(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
}

void TcpTracer::TraceInFlight(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

int main(int argc, char *argv[]) {
    std::string tcpVariant = "TcpWestwood";
    std::string bandwidth = "1Mbps";
    std::string delay = "10ms";
    double packetErrorRate = 0.0;
    uint32_t maxBytes = 0;
    bool usePcap = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpWestwood or TcpWestwoodPlus", tcpVariant);
    cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
    cmd.AddValue("delay", "Bottleneck delay", delay);
    cmd.AddValue("packetErrorRate", "Packet error rate on bottleneck link", packetErrorRate);
    cmd.AddValue("maxBytes", "Total number of bytes to send", maxBytes);
    cmd.AddValue("usePcap", "Enable pcap output", usePcap);
    cmd.Parse(argc, argv);

    if (tcpVariant == "TcpWestwood") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
    } else if (tcpVariant == "TcpWestwoodPlus") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        Config::SetDefault("ns3::TcpWestwood::UseEifelAlgorithm", BooleanValue(true));
    }

    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue(bandwidth));
    Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue(delay));
    Config::SetDefault("ns3::DropTailQueueBase::MaxSize", StringValue("100p"));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetConstantRate(DataRate("500kbps"));
    client.SetAttribute("PacketSize", UintegerValue(1000));
    if (maxBytes > 0)
        client.SetAttribute("MaxBytes", UintegerValue(maxBytes));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    if (usePcap) {
        pointToPoint.EnablePcapAll("tcp-westwood-sim");
    }

    TcpTracer tracer;

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("cwnd-trace.txt");
    Ptr<OutputStreamWrapper> ssthreshStream = asciiTraceHelper.CreateFileStream("ssthresh-trace.txt");
    Ptr<OutputStreamWrapper> rttStream = asciiTraceHelper.CreateFileStream("rtt-trace.txt");
    Ptr<OutputStreamWrapper> rtoStream = asciiTraceHelper.CreateFileStream("rto-trace.txt");
    Ptr<OutputStreamWrapper> inFlightStream = asciiTraceHelper.CreateFileStream("inflight-trace.txt");

    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeBoundCallback(&TcpTracer::TraceCongestionWindow, &tracer, cwndStream));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/SlowStartThreshold", MakeBoundCallback(&TcpTracer::TraceSlowStartThreshold, &tracer, ssthreshStream));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTT", MakeBoundCallback(&TcpTracer::TraceRtt, &tracer, rttStream));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTO", MakeBoundCallback(&TcpTracer::TraceRto, &tracer, rtoStream));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/BytesInFlight", MakeBoundCallback(&TcpTracer::TraceInFlight, &tracer, inFlightStream));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}