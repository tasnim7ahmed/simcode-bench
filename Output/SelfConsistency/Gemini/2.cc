#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointExample");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    LogComponentEnable("PointToPointExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    NS_LOG_INFO("Point-to-point link created between nodes 0 and 1.");

    // Optional: Add a callback to verify the link state (not strictly required for basic setup)

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}