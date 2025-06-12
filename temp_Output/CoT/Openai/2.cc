#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Verification: check that both devices and channel were installed
    NS_ASSERT(devices.GetN() == 2);
    Ptr<Channel> channel0 = devices.Get(0)->GetChannel();
    Ptr<Channel> channel1 = devices.Get(1)->GetChannel();
    NS_ASSERT(channel0 != nullptr && channel1 != nullptr);
    NS_ASSERT(channel0 == channel1);

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}