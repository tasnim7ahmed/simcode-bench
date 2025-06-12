#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify setup: Ensure two devices are installed
    if (devices.GetN() != 2)
    {
        NS_FATAL_ERROR("Point-to-point link was not correctly established.");
    }

    // Optionally, check channel pointer is valid
    Ptr<Channel> channel = devices.Get(0)->GetChannel();
    if (channel == nullptr)
    {
        NS_FATAL_ERROR("Channel not attached to device 0.");
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}