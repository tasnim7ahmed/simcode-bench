#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer iotNodes;
  iotNodes.Create(5);

  NodeContainer allNodes;
  allNodes.Add(serverNode);
  allNodes.Add(iotNodes);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(allNodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(lrWpanDevices);

  InternetStackHelper internetv6;
  internetv6.Install(allNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(sixLowPanDevices);

  // Set up static routing for the server.  All nodes route to server
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(serverNode.Get(0)->GetObject<Ipv6> ());
  staticRouting->SetDefaultRoute(interfaces.GetAddress(1,1), 0); // first iot node
  for(uint32_t i=2; i<6; ++i){ // remaining iot nodes
      staticRouting->AddHostRouteTo(interfaces.GetAddress(i,1), interfaces.GetAddress(1,1), 0);
  }
  // Set up static routing for the iot devices. All nodes route to server
    for(uint32_t i=1; i<6; ++i){
        Ptr<Ipv6StaticRouting> staticRoutingIoT = ipv6RoutingHelper.GetOrCreateRouting(iotNodes.Get(i-1)->GetObject<Ipv6> ());
        staticRoutingIoT->SetDefaultRoute(interfaces.GetAddress(0,1), 0);
    }

  UdpServerHelper udpServer(50000);
  ApplicationContainer serverApp = udpServer.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper udpClient(interfaces.GetAddress(0,1), 50000);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  udpClient.SetAttribute("PacketSize", UintegerValue(10));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 5; ++i) {
    clientApps.Add(udpClient.Install(iotNodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  AnimationInterface anim("6lowpan-iot-simulation.xml");
  anim.SetConstantPosition(serverNode.Get(0), 10.0, 10.0);
  for(uint32_t i = 0; i < 5; ++i) {
      anim.SetConstantPosition(iotNodes.Get(i), i * 10, 0.0);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}