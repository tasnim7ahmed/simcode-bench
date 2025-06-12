#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/fat-tree-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeDataCenterSimulation");

int main(int argc, char *argv[]) {
    uint16_t k = 4;
    uint16_t totalNodes = 0;
    std::string animFile = "fat_tree_data_center.xml";
    Time stopTime = Seconds(10.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("k", "Fat-Tree parameter (even number)", k);
    cmd.AddValue("stopTime", "Duration of the simulation", stopTime);
    cmd.Parse(argc, argv);

    if (k % 2 != 0) {
        NS_FATAL_ERROR("Fat-Tree parameter 'k' must be an even number.");
    }

    FatTreeHelper fatTree(k);
    NodeContainer nodes = fatTree.Build();
    totalNodes = nodes.GetN();

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        for (uint32_t j = 0; j < nodes.GetN(); ++j) {
            if (i != j && fatTree.AreNodesConnected(nodes.Get(i), nodes.Get(j))) {
                NetDeviceContainer pointToPointDevices = p2p.Install(nodes.Get(i), nodes.Get(j));
                devices.Add(pointToPointDevices);
            }
        }
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 5000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < totalNodes; ++i) {
        for (uint32_t j = 0; j < totalNodes; ++j) {
            if (i != j) {
                Ptr<Node> sender = nodes.Get(i);
                Ptr<Node> receiver = nodes.Get(j);

                BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(j, 0), port));
                source.SetAttribute("MaxBytes", UintegerValue(1000000));
                ApplicationContainer sourceApp = source.Install(sender);
                sourceApp.Start(Seconds(1.0));
                sourceApp.Stop(stopTime);

                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                ApplicationContainer sinkApp = sink.Install(receiver);
                sinkApp.Start(Seconds(1.0));
                sinkApp.Stop(stopTime);

                serverApps.Add(sinkApp);
                clientApps.Add(sourceApp);

                port++;
            }
        }
    }

    AnimationInterface anim(animFile);
    anim.SetStartTime(Seconds(0.0));
    anim.SetStopTime(stopTime);

    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}