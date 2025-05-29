#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/zigbee-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ZigbeeWsnExample");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  const uint32_t numNodes = 10;
  const double simTime = 10.0;
  const double areaSize = 100.0;
  const double appStart = 1.0;

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Install mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", TimeValue (Seconds (0.5)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", RectangleValue (Rectangle (0.0, areaSize, 0.0, areaSize)));
  mobility.Install (nodes);

  // Install LR-WPAN (IEEE 802.15.4)
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devs = lrWpanHelper.Install (nodes);
  lrWpanHelper.AssociateToPan (devs, 0xAAAA);

  // Enable ZigBee PRO mode
  ZigbeeHelper zigbeeHelper;
  zigbeeHelper.SetStandard (ZigbeeHelper::ZB_PRO);
  ZigbeeDeviceContainer zbDevices = zigbeeHelper.Install (devs);

  // Assign coordinator/end device roles
  Ptr<ZigbeeNetDevice> coordinatorZb = zbDevices.Get (0)->GetObject<ZigbeeNetDevice> ();
  coordinatorZb->SetDeviceType (ZIGBEE_DEVICE_TYPE_COORDINATOR);
  coordinatorZb->SetNetworkAddress (ZigbeeAddress (0x0000));
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      Ptr<ZigbeeNetDevice> dev = zbDevices.Get (i)->GetObject<ZigbeeNetDevice> ();
      dev->SetDeviceType (ZIGBEE_DEVICE_TYPE_END_DEVICE);
      dev->SetNetworkAddress (ZigbeeAddress (0x0001 + i));
    }

  // Enable PCAP tracing
  lrWpanHelper.EnablePcapAll (std::string ("zigbee-wsn"));

  // Install internet stack (6LoWPAN for IPv6 over ZigBee)
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer lowpanDevices = sixLowPanHelper.Install (devs);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifcs = ipv6.Assign (lowpanDevices);
  for (uint32_t i = 0; i < ifcs.GetN (); ++i)
    {
      ifcs.SetForwarding (i, true);
      ifcs.SetDefaultRouteInAllNodes (i);
    }

  // UDP server on coordinator
  uint16_t udpPort = 4000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApps = udpServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (0));
  serverApps.Stop (Seconds (simTime));

  // UDP client on each end device
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      UdpClientHelper udpClient (ifcs.GetAddress (0, 1), udpPort);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
      udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (500)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (128));
      ApplicationContainer clientApps = udpClient.Install (nodes.Get (i));
      clientApps.Start (Seconds (appStart));
      clientApps.Stop (Seconds (simTime));
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}