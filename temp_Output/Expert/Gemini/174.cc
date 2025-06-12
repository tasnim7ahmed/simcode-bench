#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer ringNodes;
  ringNodes.Create(5);

  NodeContainer meshNodes;
  meshNodes.Create(4);

  NodeContainer busNodes;
  busNodes.Create(3);

  NodeContainer lineNodes;
  lineNodes.Create(3);

  NodeContainer starNode;
  starNode.Create(1);
  NodeContainer starLeaves;
  starLeaves.Create(3);

  NodeContainer treeRoot;
  treeRoot.Create(1);
  NodeContainer treeLevel1;
  treeLevel1.Create(2);
  NodeContainer treeLevel2;
  treeLevel2.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer ringDevices;
  for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
    NetDeviceContainer devices = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
    ringDevices.Add(devices);
  }

  NetDeviceContainer meshDevices;
  for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
      for (uint32_t j = i + 1; j < meshNodes.GetN(); ++j) {
          NetDeviceContainer devices = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
          meshDevices.Add(devices);
      }
  }

  NetDeviceContainer busDevices;
  for (uint32_t i = 0; i < busNodes.GetN() - 1; ++i) {
      NetDeviceContainer devices = p2p.Install(busNodes.Get(i), busNodes.Get(i + 1));
      busDevices.Add(devices);
  }

  NetDeviceContainer lineDevices;
  for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i) {
      NetDeviceContainer devices = p2p.Install(lineNodes.Get(i), lineNodes.Get(i + 1));
      lineDevices.Add(devices);
  }

  NetDeviceContainer starDevices;
  for (uint32_t i = 0; i < starLeaves.GetN(); ++i) {
      NetDeviceContainer devices = p2p.Install(starNode.Get(0), starLeaves.Get(i));
      starDevices.Add(devices);
  }

    NetDeviceContainer treeDevicesRootToLevel1_0 = p2p.Install(treeRoot.Get(0), treeLevel1.Get(0));
    NetDeviceContainer treeDevicesRootToLevel1_1 = p2p.Install(treeRoot.Get(0), treeLevel1.Get(1));
    NetDeviceContainer treeDevicesLevel1_0_ToLevel2_0 = p2p.Install(treeLevel1.Get(0), treeLevel2.Get(0));
    NetDeviceContainer treeDevicesLevel1_0_ToLevel2_1 = p2p.Install(treeLevel1.Get(0), treeLevel2.Get(1));
    NetDeviceContainer treeDevicesLevel1_1_ToLevel2_2 = p2p.Install(treeLevel1.Get(1), treeLevel2.Get(2));
    NetDeviceContainer treeDevicesLevel1_1_ToLevel2_3 = p2p.Install(treeLevel1.Get(1), treeLevel2.Get(3));

  InternetStackHelper internet;
  internet.Install(ringNodes);
  internet.Install(meshNodes);
  internet.Install(busNodes);
  internet.Install(lineNodes);
  internet.Install(starNode);
  internet.Install(starLeaves);
  internet.Install(treeRoot);
  internet.Install(treeLevel1);
  internet.Install(treeLevel2);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces = address.Assign(ringDevices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces = address.Assign(busDevices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer lineInterfaces = address.Assign(lineDevices);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces = address.Assign(starDevices);

  address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfacesRootToLevel1_0 = address.Assign(treeDevicesRootToLevel1_0);
    address.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfacesRootToLevel1_1 = address.Assign(treeDevicesRootToLevel1_1);
    address.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfacesLevel1_0_ToLevel2_0 = address.Assign(treeDevicesLevel1_0_ToLevel2_0);
    address.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfacesLevel1_0_ToLevel2_1 = address.Assign(treeDevicesLevel1_0_ToLevel2_1);
    address.SetBase("10.1.10.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfacesLevel1_1_ToLevel2_2 = address.Assign(treeDevicesLevel1_1_ToLevel2_2);
    address.SetBase("10.1.11.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfacesLevel1_1_ToLevel2_3 = address.Assign(treeDevicesLevel1_1_ToLevel2_3);

  OlsrHelper olsr;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApp = sinkHelper.Install (treeLevel2.Get (3));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  uint16_t port = 9;
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", (InetSocketAddress (treeLevel2.Get(3)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port)));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  ApplicationContainer app = onOffHelper.Install (ringNodes.Get(0));
  app.Start (Seconds (2.0));
  app.Stop (Seconds (10.0));

  AnimationInterface anim ("hybrid-topology.xml");
  anim.EnablePacketMetadata (true);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}