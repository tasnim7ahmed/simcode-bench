#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-westwood.h"
#include <map>
#include <fstream>
#include <iomanip>

using namespace ns3;

// Tracing maps and output files
static std::map<uint32_t, std::ofstream> cwndStreams;
static std::map<uint32_t, std::ofstream> ssthreshStreams;
static std::map<uint32_t, std::ofstream> rttStreams;
static std::map<uint32_t, std::ofstream> rtoStreams;
static std::map<uint32_t, std::ofstream> inflightStreams;

void
CwndTracer(uint32_t oldCwnd, uint32_t newCwnd, Ptr<const TcpSocketBase> socket)
{
    uint32_t nodeId = socket->GetNode()->GetId();
    cwndStreams[nodeId] << Simulator::Now().GetSeconds()
                        << " " << newCwnd << std::endl;
}

void
SsthreshTracer(uint32_t oldSsthresh, uint32_t newSsthresh, Ptr<const TcpSocketBase> socket)
{
    uint32_t nodeId = socket->GetNode()->GetId();
    ssthreshStreams[nodeId] << Simulator::Now().GetSeconds()
                            << " " << newSsthresh << std::endl;
}

void
RttTracer(Time oldRtt, Time newRtt, Ptr<const TcpSocketBase> socket)
{
    uint32_t nodeId = socket->GetNode()->GetId();
    rttStreams[nodeId] << Simulator::Now().GetSeconds()
                       << " " << newRtt.GetMilliSeconds() << std::endl;
}

void
RtoTracer(Time oldRto, Time newRto, Ptr<const TcpSocketBase> socket)
{
    uint32_t nodeId = socket->GetNode()->GetId();
    rtoStreams[nodeId] << Simulator::Now().GetSeconds()
                       << " " << newRto.GetMilliSeconds() << std::endl;
}

void
InflightTracer(uint32_t oldBytes, uint32_t newBytes, Ptr<const TcpSocketBase> socket)
{
    uint32_t nodeId = socket->GetNode()->GetId();
    inflightStreams[nodeId] << Simulator::Now().GetSeconds()
                            << " " << newBytes << std::endl;
}

void
SetupSocketTracing(Ptr<Node> node, Ptr<Socket> sock)
{
    Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(sock);
    if (!tcp)
        return;

    uint32_t nodeId = node->GetId();

    if (cwndStreams.find(nodeId) == cwndStreams.end())
    {
        std::ostringstream fn;
        fn << "cwnd-node" << nodeId << ".dat";
        cwndStreams[nodeId].open(fn.str());
    }
    if (ssthreshStreams.find(nodeId) == ssthreshStreams.end())
    {
        std::ostringstream fn;
        fn << "ssthresh-node" << nodeId << ".dat";
        ssthreshStreams[nodeId].open(fn.str());
    }
    if (rttStreams.find(nodeId) == rttStreams.end())
    {
        std::ostringstream fn;
        fn << "rtt-node" << nodeId << ".dat";
        rttStreams[nodeId].open(fn.str());
    }
    if (rtoStreams.find(nodeId) == rtoStreams.end())
    {
        std::ostringstream fn;
        fn << "rto-node" << nodeId << ".dat";
        rtoStreams[nodeId].open(fn.str());
    }
    if (inflightStreams.find(nodeId) == inflightStreams.end())
    {
        std::ostringstream fn;
        fn << "inflight-node" << nodeId << ".dat";
        inflightStreams[nodeId].open(fn.str());
    }

    tcp->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndTracer, tcp));
    tcp->TraceConnectWithoutContext("SlowStartThreshold", MakeBoundCallback(&SsthreshTracer, tcp));
    tcp->TraceConnectWithoutContext("RTT", MakeBoundCallback(&RttTracer, tcp));
    tcp->TraceConnectWithoutContext("RTO", MakeBoundCallback(&RtoTracer, tcp));
    tcp->TraceConnectWithoutContext("BytesInFlight", MakeBoundCallback(&InflightTracer, tcp));
}

void
TraceSocketsAtApps(ApplicationContainer apps)
{
    for (uint32_t i = 0; i < apps.GetN(); ++i)
    {
        Ptr<Application> app = apps.Get(i);
        Ptr<BulkSendApplication> bsa = DynamicCast<BulkSendApplication>(app);
        if (bsa)
        {
            Simulator::Schedule(Seconds(0.0), [app, bsa]()
            {
                Ptr<Socket> sock = bsa->GetSocket();
                if (sock)
                {
                    SetupSocketTracing(app->GetNode(), sock);
                }
            });
        }
    }
}

void
ConfigureTcpVariant(std::string variant)
{
    if (variant == "TcpWestwoodPlus")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
    }
    else if (variant == "TcpWestwood")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOOD));
    }
    else
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
    }
}

NetDeviceContainer
InstallPointToPointLink(NodeContainer nodes, std::string bandwidth, std::string delay, double per)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(delay));
    NetDeviceContainer devices = p2p.Install(nodes);

    // Optional error model
    if (per > 0.0)
    {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(per));
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }
    return devices;
}

static void
InstallBulkSendAndSink(NodeContainer &nodes,
                       Ipv4AddressHelper &address,
                       uint16_t port,
                       uint64_t maxBytes,
                       ApplicationContainer &sourceApps,
                       ApplicationContainer &sinkApps)
{
    // Assign address after installing NetDevices
    Ipv4InterfaceContainer interfaces = address.Assign(nodes.Get(0)->GetDevice(0), nodes.Get(1)->GetDevice(0));
    Ipv4Address remoteAddr = interfaces.GetAddress(1);
    // BulkSend
    BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (remoteAddr, port));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(20.0));
    sourceApps.Add(sourceApp);

    // PacketSink
    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.5));
    sinkApp.Stop(Seconds(21.0));
    sinkApps.Add(sinkApp);
}

int main(int argc, char *argv[])
{
    double simDuration = 20.0;
    std::string bandwidth = "10Mbps";
    std::string delay = "50ms";
    double per = 0.0; // Packet error rate
    std::string tcpVariant = "TcpWestwoodPlus";
    uint32_t nFlows = 1;
    uint16_t basePort = 50000;
    uint64_t maxBytes = 0; // unlimited

    CommandLine cmd;
    cmd.AddValue("bandwidth", "Link bandwidth", bandwidth);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("per", "Packet error rate", per);
    cmd.AddValue("tcpVariant", "TCP variant (TcpWestwood, TcpWestwoodPlus)", tcpVariant);
    cmd.AddValue("nFlows", "Number of simultaneous flows", nFlows);
    cmd.AddValue("maxBytes", "Maximum bytes to send per BulkSend app (0 = unlimited)", maxBytes);
    cmd.AddValue("simDuration", "Simulation duration (seconds)", simDuration);
    cmd.Parse(argc, argv);

    ConfigureTcpVariant(tcpVariant);

    NodeContainer allNodes;
    std::vector<NodeContainer> flowNodes;
    InternetStackHelper internet;

    allNodes.Create(2 * nFlows); // Each flow has 2 nodes

    for (uint32_t i = 0; i < 2 * nFlows; ++i)
        internet.Install(allNodes.Get(i));

    std::vector<NetDeviceContainer> devicesList;
    std::vector<Ipv4AddressHelper> addressList;
    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    for (uint32_t i = 0; i < nFlows; ++i)
    {
        NodeContainer pair;
        pair.Add(allNodes.Get(2 * i));
        pair.Add(allNodes.Get(2 * i + 1));
        flowNodes.push_back(pair);

        NetDeviceContainer devices = InstallPointToPointLink(pair, bandwidth, delay, per);
        devicesList.push_back(devices);

        std::ostringstream subnet;
        subnet << "10." << i + 1 << ".1.0";
        Ipv4AddressHelper address;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        addressList.push_back(address);

        Ipv4InterfaceContainer iface = address.Assign(devices);
    }

    // Set up TCP applications per flow
    for (uint32_t i = 0; i < nFlows; ++i)
    {
        InstallBulkSendAndSink(flowNodes[i],
                               addressList[i],
                               basePort + i,
                               maxBytes,
                               sourceApps,
                               sinkApps);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Tracing
    TraceSocketsAtApps(sourceApps);

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    for (auto &kv : cwndStreams) kv.second.close();
    for (auto &kv : ssthreshStreams) kv.second.close();
    for (auto &kv : rttStreams) kv.second.close();
    for (auto &kv : rtoStreams) kv.second.close();
    for (auto &kv : inflightStreams) kv.second.close();

    return 0;
}