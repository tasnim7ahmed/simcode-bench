#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopology");

std::ofstream outputFile;

void ReceivePacket(std::string context, Ptr<const Packet> p, const Address &addr) {
    Time arrivalTime = Simulator::Now();
    Ipv4Address senderAddress = InetSocketAddress::ConvertFrom(addr).GetIpv4();

    uint32_t sourceNodeId = 0;
    uint32_t destinationNodeId = 0;

    // Extract source and destination node ID from the context string
    size_t pos1 = context.find("/$ns3::UdpClient[");
    if (pos1 != std::string::npos) {
        size_t start = pos1 + 17;
        size_t end = context.find("]", start);
        if (end != std::string::npos) {
            try {
                sourceNodeId = std::stoul(context.substr(start, end - start));
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument: " << ia.what() << '\n';
            } catch (const std::out_of_range& oor) {
                std::cerr << "Out of Range error: " << oor.what() << '\n';
            }
        }
    }
   
    size_t pos2 = context.find("/$ns3::UdpServer[");
     if (pos2 != std::string::npos) {
        size_t start = pos2 + 17;
        size_t end = context.find("]", start);
        if (end != std::string::npos) {
            try {
                destinationNodeId = std::stoul(context.substr(start, end - start));
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument: " << ia.what() << '\n';
            } catch (const std::out_of_range& oor) {
                std::cerr << "Out of Range error: " << oor.what() << '\n';
            }
        }
    }

    outputFile << sourceNodeId << ","
               << destinationNodeId << ","
               << p->GetSize() << ","
               << Simulator::Now().GetSeconds() - (double(p->GetUid()) / 1000000000.0)<< "," // Approximate TX time
               << arrivalTime.GetSeconds() << std::endl;
}


int main(int argc, char *argv[]) {
    LogComponentEnable("RingTopology", LOG_LEVEL_INFO);

    uint32_t numNodes = 4;
    double simulationTime = 10.0;
    std::string animFile = "ring-animation.xml";

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the ring", numNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("animFile", "File Name for Animation Output", animFile);

    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[numNodes];
    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t nextNode = (i + 1) % numNodes;
        NodeContainer linkNodes;
        linkNodes.Add(nodes.Get(i));
        linkNodes.Add(nodes.Get(nextNode));
        devices[i] = p2p.Install(linkNodes);
    }


    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[numNodes];
    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t nextNode = (i + 1) % numNodes;
        NodeContainer linkNodes;
        linkNodes.Add(nodes.Get(i));
        linkNodes.Add(nodes.Get(nextNode));

        ipv4.Assign(devices[i]);
        interfaces[i] = ipv4.Assign(devices[i]);
        ipv4.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t nextNode = (i + 1) % numNodes;

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(nextNode));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simulationTime));

        UdpClientHelper client(interfaces[i].GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295));
        client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simulationTime));

        // Trace sink at the server
        std::stringstream serverTracePath;
        serverTracePath << "/NodeList/" << nextNode << "/ApplicationList/*/$ns3::UdpServer/Rx";
        Config::Connect(serverTracePath.str(), MakeCallback(&ReceivePacket));
    }

    outputFile.open("ring_data.csv");
    outputFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;


    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
      anim.SetNodePosition(nodes.Get(i), 10 + (i * 20), 10);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    outputFile.close();

    Simulator::Destroy();
    return 0;
}