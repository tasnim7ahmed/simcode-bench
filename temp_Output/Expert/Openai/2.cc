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

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    NS_ASSERT_MSG(devices.GetN() == 2, "Exactly two net devices should be installed");
    Ptr<NetDevice> dev0 = devices.Get(0);
    Ptr<NetDevice> dev1 = devices.Get(1);
    NS_ASSERT_MSG(dev0 != nullptr && dev1 != nullptr, "NetDevices should be valid");

    Ptr<Channel> chan0 = dev0->GetChannel();
    Ptr<Channel> chan1 = dev1->GetChannel();
    NS_ASSERT_MSG(chan0 != nullptr && chan1 != nullptr, "Channels should be valid");
    NS_ASSERT_MSG(chan0 == chan1, "Both devices should be on the same channel");

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}