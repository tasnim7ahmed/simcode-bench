#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomNetworkSimulation");

int main(int argc, char *argv[]) {
    LogComponentEnable("CustomNetworkSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    std::string matrixFile = "adjacency_matrix.txt";
    std::string coordFile = "node_coordinates.txt";
    std::string animFile = "network-animation.xml";
    double simDuration = 10.0;
    double cbrInterval = 0.001; // seconds
    uint32_t cbrPacketSize = 512; // bytes

    CommandLine cmd;
    cmd.AddValue("matrixFile", "Adjacency matrix input file name", matrixFile);
    cmd.AddValue("coordFile", "Node coordinates input file name", coordFile);
    cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
    cmd.Parse(argc, argv);

    // Read adjacency matrix
    std::ifstream matIn(matrixFile);
    uint32_t n;
    matIn >> n;
    std::vector<std::vector<uint32_t>> adjMatrix(n, std::vector<uint32_t>(n));
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            matIn >> adjMatrix[i][j];
        }
    }

    // Read node coordinates
    std::ifstream coordIn(coordFile);
    std::vector<std::pair<double, double>> coords(n);
    for (uint32_t i = 0; i < n; ++i) {
        coordIn >> coords[i].first >> coords[i].second;
    }

    NodeContainer nodes;
    nodes.Create(n);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    std::vector<NetDeviceContainer> devices(n);
    std::vector<Ipv4InterfaceContainer> interfaces(n);

    uint32_t subnet = 1;
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j] != 0) {
                NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
                NetDeviceContainer dev = p2p.Install(pair);
                std::ostringstream subnetStream;
                subnetStream << "10.1." << subnet++ << ".0";
                address.SetBase(subnetStream.str().c_str(), "255.255.255.0");
                Ipv4InterfaceContainer ip = address.Assign(dev);
                devices[i].Add(dev.Get(0));
                devices[j].Add(dev.Get(1));
                interfaces[i].Add(ip.Get(0));
                interfaces[j].Add(ip.Get(1));
            }
        }
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    for (uint32_t i = 0; i < n; ++i) {
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(coords[i].first, coords[i].second, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.Install(nodes.Get(i));
    }

    AnimationInterface anim(animFile);
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(simDuration), Seconds(0.25));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    for (uint32_t src = 0; src < n; ++src) {
        for (uint32_t dst = 0; dst < n; ++dst) {
            if (src != dst && interfaces[src].GetN() > 0) {
                Ipv4Address dstAddr = interfaces[dst].GetAddress(0);
                UdpEchoClientHelper echoClient(dstAddr, 9);
                echoClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(cbrInterval)));
                echoClient.SetAttribute("PacketSize", UintegerValue(cbrPacketSize));
                ApplicationContainer clientApps = echoClient.Install(nodes.Get(src));
                clientApps.Start(Seconds(1.0));
                clientApps.Stop(Seconds(simDuration - 1.0));
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}