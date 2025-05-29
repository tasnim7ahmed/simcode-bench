#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and channel between the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify link setup by checking device count and channel assignment
    NS_ASSERT_MSG(devices.GetN() == 2, "Should have two net devices installed");

    Ptr<Channel> channel0 = devices.Get(0)->GetChannel();
    Ptr<Channel> channel1 = devices.Get(1)->GetChannel();
    NS_ASSERT_MSG(channel0 != 0 && channel1 != 0, "Each device should be assigned a channel");
    NS_ASSERT_MSG(channel0 == channel1, "Both devices should be connected to the same channel");

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}