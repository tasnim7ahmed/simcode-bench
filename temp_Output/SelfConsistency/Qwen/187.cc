#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/fat-tree-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DataCenterFatTreeSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Simulation parameters
    uint16_t k = 4; // Fat-Tree parameter (number of ports per switch)
    Time simulationTime = Seconds(10.0);

    // Create Fat-Tree topology helper
    FatTreeHelper fatTree(k);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    fatTree.InstallStack(internet);

    // Assign IP addresses to all interfaces
    fatTree.AssignIpv4Addresses();

    // Setup TCP connections between servers
    uint32_t numHosts = fatTree.GetHostCount();
    NS_ASSERT_MSG(numHosts >= 2, "Need at least two hosts for communication");

    // Use BulkSendApplication over TCP to send data from host 0 to host 1
    uint16_t port = 5000;
    BulkSendHelper senderHelper("ns3::TcpSocketFactory", InetSocketAddress(fatTree.GetHostAddress(1), port));
    senderHelper.SetAttribute("MaxBytes", UintegerValue(10485760)); // Send 10MB

    ApplicationContainer senderApp = senderHelper.Install(fatTree.GetHost(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(simulationTime);

    // PacketSink on host 1 to receive the data
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(fatTree.GetHost(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(simulationTime);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup Point-to-Point links with default Data Rate and Delay
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // NetAnim visualization setup
    AnimationInterface anim("fat-tree-animation.xml");
    anim.SetConstantPosition(fatTree.GetHost(0)->GetId(), 0.0, 0.0);
    anim.SetConstantPosition(fatTree.GetHost(1)->GetId(), 10.0, 0.0);
    // Additional positions can be added for more detailed visualization

    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}