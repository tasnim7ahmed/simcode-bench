#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>

using namespace ns3;

std::vector<std::vector<int>> ReadAdjacencyMatrix(const std::string& filename, uint32_t& numNodes) {
    std::ifstream infile(filename);
    std::string line;
    std::vector<std::vector<int>> matrix;
    std::vector<std::string> lines;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        lines.push_back(line);
    }
    numNodes = lines.size();
    // Fill matrix with zeros
    matrix.resize(numNodes, std::vector<int>(numNodes, 0));
    for (uint32_t i = 0; i < numNodes; ++i) {
        std::istringstream iss(lines[i]);
        for (uint32_t j = i; j < numNodes; ++j) {
            int val;
            if (!(iss >> val)) {
                val = 0;
            }
            matrix[i][j] = val;
            matrix[j][i] = val; // since undirected
        }
    }
    return matrix;
}

std::vector<std::vector<double>> ReadCoordinates(const std::string& filename, uint32_t numNodes) {
    std::ifstream infile(filename);
    std::vector<std::vector<double>> coords(numNodes, std::vector<double>(2, 0.0));
    std::string line;
    uint32_t idx = 0;
    while (std::getline(infile, line) && idx < numNodes) {
        std::istringstream iss(line);
        double x, y;
        iss >> x >> y;
        coords[idx][0] = x;
        coords[idx][1] = y;
        ++idx;
    }
    return coords;
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    std::string adjFile = "adjacency_matrix.txt";
    std::string coordFile = "node_coordinates.txt";
    std::string animFile = "netanim.xml";
    cmd.AddValue("adj", "Adjacency matrix file", adjFile);
    cmd.AddValue("coord", "Node coordinates file", coordFile);
    cmd.AddValue("anim", "NetAnim output file", animFile);
    cmd.Parse(argc, argv);

    uint32_t numNodes = 0;
    std::vector<std::vector<int>> adjacency = ReadAdjacencyMatrix(adjFile, numNodes);
    std::vector<std::vector<double>> coords = ReadCoordinates(coordFile, numNodes);

    NodeContainer nodes;
    nodes.Create(numNodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Maintain links and interfaces
    std::vector<NetDeviceContainer> allDevices;
    std::vector<std::pair<uint32_t,uint32_t>> linkPairs;
    std::vector<Ipv4InterfaceContainer> allInterfaces;

    Ipv4AddressHelper address;
    uint32_t netNum = 1;

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i+1; j < numNodes; ++j) {
            if (adjacency[i][j]) {
                NodeContainer nc = NodeContainer(nodes.Get(i), nodes.Get(j));
                NetDeviceContainer ndc = p2p.Install(nc);
                allDevices.push_back(ndc);
                linkPairs.push_back(std::make_pair(i,j));
                std::ostringstream subnet;
                subnet << "10." << netNum << ".0.0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");
                allInterfaces.push_back(address.Assign(ndc));
                netNum++;
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Assign mobility/static positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes; ++i) {
        posAlloc->Add(Vector(coords[i][0], coords[i][1], 0.0));
    }
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // CBR Applications: Each node => every other node (n*(n-1) flows)
    uint16_t portBase = 4000;
    double appStart = 1.0;
    double appStop = 19.0;

    uint32_t interfaceIndex = 0;
    std::vector<Ipv4Address> nodeIpAddresses(numNodes, Ipv4Address("0.0.0.0"));

    // Find any interface IP for each node (from attached interfaces)
    Ptr<Ipv4> ipv4;
    for (uint32_t i = 0; i < numNodes; ++i) {
        ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        for (uint32_t j = 1; j < ipv4->GetNInterfaces(); ++j) { // interface 0 is loopback
            if (ipv4->GetAddress(j,0) != Ipv4Address("0.0.0.0")) {
                nodeIpAddresses[i] = ipv4->GetAddress(j,0).GetLocal();
                break;
            }
        }
    }

    ApplicationContainer appCont;

    for (uint32_t src = 0; src < numNodes; ++src) {
        for (uint32_t dst = 0; dst < numNodes; ++dst) {
            if (src == dst) continue;
            // Install receiver on dst (if not already)
            uint16_t port = portBase + src; // For uniqueness, per SRC->DST, can fine-tune this.
            OnOffHelper onoff("ns3::UdpSocketFactory", 
                              InetSocketAddress(nodeIpAddresses[dst], port));
            onoff.SetAttribute("DataRate", StringValue("2Mbps"));
            onoff.SetAttribute("PacketSize", UintegerValue(1024));
            onoff.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
            onoff.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
            ApplicationContainer tempApp = onoff.Install(nodes.Get(src));
            appCont.Add(tempApp);

            // Install sink on dst
            Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
            PacketSinkHelper sink("ns3::UdpSocketFactory", sinkAddress);
            ApplicationContainer sinkApp = sink.Install(nodes.Get(dst));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(appStop+1.0));
            appCont.Add(sinkApp);
        }
    }

    // Enable logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // NetAnim
    AnimationInterface anim(animFile);
    anim.SetBackgroundImage("background.png", 0,0, 800,600,1.0);

    for (uint32_t i = 0; i < numNodes; ++i) {
        anim.SetConstantPosition(nodes.Get(i), coords[i][0], coords[i][1]);
    }
    for (size_t k = 0; k < linkPairs.size(); ++k) {
        anim.UpdateLinkDescription(allDevices[k].Get(0), "p2p", 0);
    }

    Simulator::Stop(Seconds(appStop+2.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}