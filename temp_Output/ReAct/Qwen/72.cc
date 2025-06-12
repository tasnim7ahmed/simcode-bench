#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/global-router-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoLanWanBridgeTopology");

class UdpEchoClientHelper : public ApplicationHelper
{
public:
  UdpEchoClientHelper(Ipv4Address address, uint16_t port);
};

UdpEchoClientHelper::UdpEchoClientHelper(Ipv4Address address, uint16_t port)
  : ApplicationHelper("ns3::UdpEchoClientApplication")
{
  m_factory.SetTypeId("ns3::UdpEchoClientApplication");
  SetAttribute("RemoteAddress", AddressValue(Address(address)));
  SetAttribute("RemotePort", UintegerValue(port));
  SetAttribute("MaxPackets", UintegerValue(5));
  SetAttribute("Interval", TimeValue(Seconds(1.0)));
  SetAttribute("PacketSize", UintegerValue(1024));
}

class UdpEchoServerHelper : public ApplicationHelper
{
public:
  UdpEchoServerHelper(uint16_t port);
};

UdpEchoServerHelper::UdpEchoServerHelper(uint16_t port)
  : ApplicationHelper("ns3::UdpEchoServerApplication")
{
  m_factory.SetTypeId("ns3::UdpEchoServerApplication");
  SetAttribute("Port", UintegerValue(port));
}

int main(int argc, char *argv[])
{
  std::string lanLinkType = "100Mbps";
  std::string wanDataRate = "1Mbps";
  std::string wanDelay = "20ms";

  CommandLine cmd;
  cmd.AddValue("lanLinkType", "LAN link type: 100Mbps or 10Mbps", lanLinkType);
  cmd.AddValue("wanDataRate", "WAN DataRate (e.g., 1Mbps)", wanDataRate);
  cmd.AddValue("wanDelay", "WAN Delay (e.g., 20ms)", wanDelay);
  cmd.Parse(argc, argv);

  bool use100Mbit = (lanLinkType == "100Mbps");

  NodeContainer topNodes, bottomNodes;
  topNodes.Create(4); // t2, tr, and two switches in path
  bottomNodes.Create(4); // b2, br, and two switches in path

  NodeContainer topSwitches;
  topSwitches.Create(2);
  NodeContainer bottomSwitches;
  bottomSwitches.Create(2);

  NodeContainer routers;
  routers.Add(topNodes.Get(1)); // tr
  routers.Add(bottomNodes.Get(1)); // br

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue(use100Mbit ? "100Mbps" : "10Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
  p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

  NetDeviceContainer topSwitchDevs, bottomSwitchDevs;

  // Top LAN: t2 -> sw[0] -> sw[1] -> router tr
  NetDeviceContainer t2_sw0 = csma.Install(NodeContainer(topNodes.Get(0), topSwitches.Get(0)));
  NetDeviceContainer sw0_sw1 = csma.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1)));
  NetDeviceContainer sw1_tr = csma.Install(NodeContainer(topSwitches.Get(1), routers.Get(0)));

  // Top LAN server node t3 connected directly to tr
  NetDeviceContainer t3_tr = csma.Install(NodeContainer(topNodes.Get(2), routers.Get(0)));

  // Bottom LAN: b2 -> sw[0] -> sw[1] -> router br
  NetDeviceContainer b2_sw0 = csma.Install(NodeContainer(bottomNodes.Get(0), bottomSwitches.Get(0)));
  NetDeviceContainer sw0_sw1_b = csma.Install(NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1)));
  NetDeviceContainer sw1_br = csma.Install(NodeContainer(bottomSwitches.Get(1), routers.Get(1)));

  // Bottom LAN client node b3 connected directly to br
  NetDeviceContainer b3_br = csma.Install(NodeContainer(bottomNodes.Get(2), routers.Get(1)));

  // WAN link between routers
  NetDeviceContainer wanDevices = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));

  // Bridge configurations for switches
  for (auto switchNode : topSwitches)
  {
    BridgeHelper bridge;
    NetDeviceContainer bridgePorts;
    for (uint32_t i = 0; i < switchNode->GetNDevices(); ++i)
    {
      bridgePorts.Add(switchNode->GetDevice(i));
    }
    bridge.Install(switchNode, bridgePorts);
  }

  for (auto switchNode : bottomSwitches)
  {
    BridgeHelper bridge;
    NetDeviceContainer bridgePorts;
    for (uint32_t i = 0; i < switchNode->GetNDevices(); ++i)
    {
      bridgePorts.Add(switchNode->GetDevice(i));
    }
    bridge.Install(switchNode, bridgePorts);
  }

  InternetStackHelper internet;
  internet.InstallAll();

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topInterfaces;
  topInterfaces.Add(ipv4.Assign(t2_sw0.Get(0))); // t2
  topInterfaces.Add(ipv4.Assign(sw0_sw1.Get(0))); // sw0
  topInterfaces.Add(ipv4.Assign(sw0_sw1.Get(1))); // sw1
  topInterfaces.Add(ipv4.Assign(sw1_tr.Get(0))); // tr from top side
  topInterfaces.Add(ipv4.Assign(t3_tr.Get(0))); // t3

  ipv4.SetBase("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomInterfaces;
  bottomInterfaces.Add(ipv4.Assign(b2_sw0.Get(0))); // b2
  bottomInterfaces.Add(ipv4.Assign(sw0_sw1_b.Get(0))); // sw0
  bottomInterfaces.Add(ipv4.Assign(sw0_sw1_b.Get(1))); // sw1
  bottomInterfaces.Add(ipv4.Assign(sw1_br.Get(0))); // br from bottom side
  bottomInterfaces.Add(ipv4.Assign(b3_br.Get(0))); // b3

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanInterfaces = ipv4.Assign(wanDevices);

  // Assign IP addresses to router interfaces
  Ipv4Address trWanIp = wanInterfaces.GetAddress(0);
  Ipv4Address brWanIp = wanInterfaces.GetAddress(1);

  // Set default routes
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Echo Applications
  uint16_t echoPort = 9;

  // On top LAN: t3 is server, t2 is client
  UdpEchoServerHelper topServer(echoPort);
  ApplicationContainer topServerApp = topServer.Install(topNodes.Get(2));
  topServerApp.Start(Seconds(1.0));
  topServerApp.Stop(Seconds(10.0));

  UdpEchoClientHelper topClient(topInterfaces.GetAddress(4), echoPort); // t3's IP
  ApplicationContainer topClientApp = topClient.Install(topNodes.Get(0));
  topClientApp.Start(Seconds(2.0));
  topClientApp.Stop(Seconds(10.0));

  // On bottom LAN: b2 is server, b3 is client
  UdpEchoServerHelper bottomServer(echoPort);
  ApplicationContainer bottomServerApp = bottomServer.Install(bottomNodes.Get(0));
  bottomServerApp.Start(Seconds(1.0));
  bottomServerApp.Stop(Seconds(10.0));

  UdpEchoClientHelper bottomClient(bottomInterfaces.GetAddress(4), echoPort); // b2's IP
  ApplicationContainer bottomClientApp = bottomClient.Install(bottomNodes.Get(2));
  bottomClientApp.Start(Seconds(2.0));
  bottomClientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}