#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  // Enable logging for this script if needed
  // LogComponentEnable ("PointToPointExample", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure point-to-point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install point-to-point devices and channel
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Verify that the devices are correctly installed
  NS_ASSERT(devices.GetN() == 2);
  NS_ASSERT(devices.Get(0) != nullptr);
  NS_ASSERT(devices.Get(1) != nullptr);

  // Verify that both devices are on the same channel
  Ptr<Channel> channelA = devices.Get(0)->GetChannel();
  Ptr<Channel> channelB = devices.Get(1)->GetChannel();
  NS_ASSERT(channelA != nullptr);
  NS_ASSERT(channelA == channelB);

  // Simulation does not require applications or IP, so we can schedule a short duration
  Simulator::Stop(Seconds(0.1));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}