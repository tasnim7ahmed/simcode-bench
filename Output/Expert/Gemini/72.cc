#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enablePcap = false;
  std::string lanSpeed = "100Mbps";
  std::string wanRate = "10Mbps";
  std::string wanDelay = "2ms";

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("lanSpeed", "LAN link speed", lanSpeed);
  cmd.AddValue("wanRate", "WAN link rate", wanRate);
  cmd.AddValue("wanDelay", "WAN link delay", wanDelay);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer topNodes;
  topNodes.Create(4);
  NodeContainer bottomNodes;
  bottomNodes.Create(4);

  NodeContainer topSwitches;
  topSwitches.Create(2);
  NodeContainer bottomSwitches;
  bottomSwitches.Create(2);

  NodeContainer routers;
  routers.Create(2);

  InternetStackHelper stack;
  stack.Install(topNodes);
  stack.Install(bottomNodes);
  stack.Install(routers);

  CsmaHelper csmaHelperTop;
  csmaHelperTop.SetChannelAttribute("DataRate", StringValue(lanSpeed));
  csmaHelperTop.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  CsmaHelper csmaHelperBottom;
  csmaHelperBottom.SetChannelAttribute("DataRate", StringValue(lanSpeed));
  csmaHelperBottom.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer topDev0 = csmaHelperTop.Install(NodeContainer(topNodes.Get(0), topSwitches.Get(0)));
  NetDeviceContainer topDev1 = csmaHelperTop.Install(NodeContainer(topNodes.Get(1), topSwitches.Get(0)));
  NetDeviceContainer topDev2 = csmaHelperTop.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1)));
  NetDeviceContainer topDev3 = csmaHelperTop.Install(NodeContainer(topNodes.Get(2), topSwitches.Get(1)));
  NetDeviceContainer topDev4 = csmaHelperTop.Install(NodeContainer(topNodes.Get(3), routers.Get(0)));

  NetDeviceContainer bottomDev0 = csmaHelperBottom.Install(NodeContainer(bottomNodes.Get(0), bottomSwitches.Get(0)));
  NetDeviceContainer bottomDev1 = csmaHelperBottom.Install(NodeContainer(bottomNodes.Get(1), routers.Get(1)));
  NetDeviceContainer bottomDev2 = csmaHelperBottom.Install(NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1)));
  NetDeviceContainer bottomDev3 = csmaHelperBottom.Install(NodeContainer(bottomNodes.Get(2), bottomSwitches.Get(1)));
  NetDeviceContainer bottomDev4 = csmaHelperBottom.Install(NodeContainer(bottomNodes.Get(3), bottomSwitches.Get(0)));

  PointToPointHelper pointToPointHelper;
  pointToPointHelper.SetDeviceAttribute("DataRate", StringValue(wanRate));
  pointToPointHelper.SetChannelAttribute("Delay", StringValue(wanDelay));

  NetDeviceContainer wanDevices = pointToPointHelper.Install(routers);

  BridgeHelper bridgeHelper;
  bridgeHelper.Install(topSwitches.Get(0), topDev0.Get(1));
  bridgeHelper.Install(topSwitches.Get(0), topDev1.Get(1));
  bridgeHelper.Install(topSwitches.Get(0), topDev2.Get(0));

  bridgeHelper.Install(topSwitches.Get(1), topDev2.Get(1));
  bridgeHelper.Install(topSwitches.Get(1), topDev3.Get(1));

  bridgeHelper.Install(bottomSwitches.Get(0), bottomDev0.Get(1));
  bridgeHelper.Install(bottomSwitches.Get(0), bottomDev4.Get(1));
  bridgeHelper.Install(bottomSwitches.Get(0), bottomDev2.Get(0));

  bridgeHelper.Install(bottomSwitches.Get(1), bottomDev2.Get(1));
  bridgeHelper.Install(bottomSwitches.Get(1), bottomDev3.Get(1));

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topInterfaces;
  topInterfaces = address.Assign(NetDeviceContainer(topDev0.Get(0), topDev1.Get(0), topDev3.Get(0), topDev4.Get(0)));

  address.SetBase("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomInterfaces;
  bottomInterfaces = address.Assign(NetDeviceContainer(bottomDev0.Get(0), bottomDev1.Get(0), bottomDev3.Get(0), bottomDev4.Get(0)));

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServerTop(9);
  ApplicationContainer serverAppsTop = echoServerTop.Install(topNodes.Get(2));
  serverAppsTop.Start(Seconds(1.0));
  serverAppsTop.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClientTop(topInterfaces.GetAddress(2), 9);
  echoClientTop.SetAttribute("MaxPackets", UintegerValue(1));
  echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClientTop.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientAppsTop = echoClientTop.Install(topNodes.Get(1));
  clientAppsTop.Start(Seconds(2.0));
  clientAppsTop.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServerBottom(9);
  ApplicationContainer serverAppsBottom = echoServerBottom.Install(bottomNodes.Get(1));
  serverAppsBottom.Start(Seconds(1.0));
  serverAppsBottom.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClientBottom(bottomInterfaces.GetAddress(1), 9);
  echoClientBottom.SetAttribute("MaxPackets", UintegerValue(1));
  echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClientBottom.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientAppsBottom = echoClientBottom.Install(bottomNodes.Get(2));
  clientAppsBottom.Start(Seconds(2.0));
  clientAppsBottom.Stop(Seconds(10.0));

  if (enablePcap) {
    csmaHelperTop.EnablePcapAll("top-lan");
    csmaHelperBottom.EnablePcapAll("bottom-lan");
    pointToPointHelper.EnablePcapAll("wan");
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}