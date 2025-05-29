#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"

#include <fstream>
#include <map>
#include <string>

using namespace ns3;

std::map<uint32_t, std::ofstream> cwndStreamMap;
std::map<uint32_t, std::ofstream> ssthreshStreamMap;
std::map<uint32_t, std::ofstream> rttStreamMap;
std::map<uint32_t, std::ofstream> rtoStreamMap;
std::map<uint32_t, std::ofstream> inflightStreamMap;

void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd, uint32_t socketId)
{
    double now = Simulator::Now().GetSeconds();
    if (cwndStreamMap.count(socketId))
        cwndStreamMap[socketId] << now << "\t" << newCwnd << std::endl;
}

void SsthreshTracer(uint32_t oldSsthresh, uint32_t newSsthresh, uint32_t socketId)
{
    double now = Simulator::Now().GetSeconds();
    if (ssthreshStreamMap.count(socketId))
        ssthreshStreamMap[socketId] << now << "\t" << newSsthresh << std::endl;
}

void RttTracer(Time oldRtt, Time newRtt, uint32_t socketId)
{
    double now = Simulator::Now().GetSeconds();
    if (rttStreamMap.count(socketId))
        rttStreamMap[socketId] << now << "\t" << newRtt.GetMilliSeconds() << std::endl;
}

void RtoTracer(Time oldRto, Time newRto, uint32_t socketId)
{
    double now = Simulator::Now().GetSeconds();
    if (rtoStreamMap.count(socketId))
        rtoStreamMap[socketId] << now << "\t" << newRto.GetMilliSeconds() << std::endl;
}

void InflightTracer(uint32_t oldInflight, uint32_t newInflight, uint32_t socketId)
{
    double now = Simulator::Now().GetSeconds();
    if (inflightStreamMap.count(socketId))
        inflightStreamMap[socketId] << now << "\t" << newInflight << std::endl;
}

void TraceTcp(uint32_t flowId, Ptr<Socket> socket)
{
    auto tid = socket->GetNode()->GetId() * 100 + socket->GetSocketType();
    char cwndf[64], ssthreshf[64], rttf[64], rtof[64], inflightf[64];
    snprintf(cwndf, sizeof(cwndf), "cwnd-flow%u.data", flowId);
    snprintf(ssthreshf, sizeof(ssthreshf), "ssthresh-flow%u.data", flowId);
    snprintf(rttf, sizeof(rttf), "rtt-flow%u.data", flowId);
    snprintf(rtof, sizeof(rtof), "rto-flow%u.data", flowId);
    snprintf(inflightf, sizeof(inflightf), "inflight-flow%u.data", flowId);

    cwndStreamMap[flowId] = std::ofstream(cwndf);
    ssthreshStreamMap[flowId] = std::ofstream(ssthreshf);
    rttStreamMap[flowId] = std::ofstream(rttf);
    rtoStreamMap[flowId] = std::ofstream(rtof);
    inflightStreamMap[flowId] = std::ofstream(inflightf);

    socket->TraceConnect("CongestionWindow", "", MakeBoundCallback(&CwndTracer, flowId));
    socket->TraceConnect("SlowStartThreshold", "", MakeBoundCallback(&SsthreshTracer, flowId));
    socket->TraceConnect("RTT", "", MakeBoundCallback(&RttTracer, flowId));
    socket->TraceConnect("RTO", "", MakeBoundCallback(&RtoTracer, flowId));
    socket->TraceConnect("BytesInFlight", "", MakeBoundCallback(&InflightTracer, flowId));
}

void SetupTracing(Ptr<Node> node, uint32_t flowId)
{
    Ptr<TcpL4Protocol> tcp = node->GetObject<TcpL4Protocol>();
    if (tcp)
    {
        for (auto i = tcp->SocketListBegin(); i != tcp->SocketListEnd(); ++i)
        {
            Ptr<Socket> sock = *i;
            if (sock->GetSocketType() == TcpSocketFactory::NS3_SOCK_STREAM)
            {
                TraceTcp(flowId, sock);
                break;
            }
        }
    }
}

Ptr<Socket> GetTcpSocketFromApp(Ptr<Application> app)
{
    Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication>(app);
    if (bulkApp)
        return bulkApp->GetSocket();
    return nullptr;
}

void SetupAppTracing(const std::vector<ApplicationContainer>& appsVec, const NodeContainer& senders)
{
    for (uint32_t i = 0; i < appsVec.size(); ++i)
    {
        for (uint32_t j = 0; j < appsVec[i].GetN(); ++j)
        {
            Ptr<Application> app = appsVec[i].Get(j);
            Simulator::Schedule(Seconds(0.1), [=](){
                Ptr<Socket> sock = GetTcpSocketFromApp(app);
                if (sock)
                    TraceTcp(i+1, sock);
            });
        }
    }
}

// Command line parameter defaults
double bandwidth = 10.0;        // Mbps
double delay = 10.0;            // ms
double errorRate = 0.0;         // 0 = no error
std::string tcpVariant = "TcpWestwoodPlus";
uint32_t numFlows = 2;
uint32_t packetSize = 1448;
double simTime = 10.0;
uint32_t dataRate = 50000000;

int main(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.AddValue("bandwidth", "Channel bandwidth in Mbps", bandwidth);
    cmd.AddValue("delay", "Channel delay in ms", delay);
    cmd.AddValue("errorRate", "Packet error rate [0.0-1.0]", errorRate);
    cmd.AddValue("tcpVariant", "TCP variant (TcpWestwoodPlus, TcpWestwood, ...)", tcpVariant);
    cmd.AddValue("numFlows", "Number of parallel flows", numFlows);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("packetSize", "TCP segment size", packetSize);
    cmd.Parse(argc, argv);

    std::string tcpTypeId = tcpVariant;
    if (tcpTypeId.compare("TcpWestwood") == 0)
        tcpTypeId = "ns3::TcpWestwood";
    else if (tcpTypeId.compare("TcpWestwoodPlus") == 0)
        tcpTypeId = "ns3::TcpWestwoodPlus";
    else
        tcpTypeId = "ns3::" + tcpTypeId;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(packetSize));

    NodeContainer nodes;
    nodes.Create(2 * numFlows); // flows: (sender[i] <-> receiver[i])

    InternetStackHelper stack;
    stack.Install(nodes);

    std::vector<NetDeviceContainer> devicesVec;
    std::vector<Ipv4InterfaceContainer> interfacesVec;
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        PointToPointHelper p2p;
        std::ostringstream oss_bw, oss_delay;
        oss_bw << bandwidth << "Mbps";
        oss_delay << delay << "ms";
        p2p.SetDeviceAttribute("DataRate", StringValue(oss_bw.str()));
        p2p.SetChannelAttribute("Delay", StringValue(oss_delay.str()));
        if (errorRate > 0.0)
        {
            Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
            em->SetAttribute("ErrorRate", DoubleValue(errorRate));
            p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(em));
        }

        NodeContainer n;
        n.Add(nodes.Get(i));
        n.Add(nodes.Get(i + numFlows));
        devicesVec.push_back(p2p.Install(n));
    }

    Ipv4AddressHelper address;
    std::vector<std::string> subnetVec;
    char base[20];
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        snprintf(base, sizeof(base), "10.1.%d.0", i + 1);
        address.SetBase(base, "255.255.255.0");
        interfacesVec.push_back(address.Assign(devicesVec[i]));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::vector<ApplicationContainer> sourceAppsVec(numFlows), sinkAppsVec(numFlows);

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        uint16_t port = 9000 + i;

        Address sinkAddress(InetSocketAddress(interfacesVec[i].GetAddress(1), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i + numFlows));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
        sinkAppsVec[i] = sinkApp;

        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
        sourceHelper.SetAttribute("SendSize", UintegerValue(packetSize));
        sourceHelper.SetAttribute("Remote", AddressValue(sinkAddress));
        ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(i));
        sourceApp.Start(Seconds(0.1));
        sourceApp.Stop(Seconds(simTime));
        sourceAppsVec[i] = sourceApp;
    }

    Simulator::Schedule(Seconds(0.5), SetupAppTracing, sourceAppsVec, nodes);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->SerializeToXmlFile("flowmon.xml", true, true);

    for (auto &kv : cwndStreamMap) kv.second.close();
    for (auto &kv : ssthreshStreamMap) kv.second.close();
    for (auto &kv : rttStreamMap) kv.second.close();
    for (auto &kv : rtoStreamMap) kv.second.close();
    for (auto &kv : inflightStreamMap) kv.second.close();

    Simulator::Destroy();
    return 0;
}