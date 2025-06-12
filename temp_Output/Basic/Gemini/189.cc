#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-client.h"
#include "ns3/tcp-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer csmaNodes;
  csmaNodes.Create(5);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(2)));

  NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

  InternetStackHelper stack;
  stack.Install(csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

  uint16_t port = 50000;

  // Create a server endpoint on node 4 (the last node).
  TcpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(csmaNodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create client endpoints on nodes 0, 1, 2, and 3.
  for (int i = 0; i < 4; ++i) {
    TcpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(csmaNodes.Get(i));
    clientApps.Start(Seconds(2.0 + i * 0.1));
    clientApps.Stop(Seconds(10.0));
  }

  // Add error model to one of the links
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.001));
  em->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
  csmaDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("csma-animation.xml");
  anim.SetConstantPosition(csmaNodes.Get(0), 10, 10);
  anim.SetConstantPosition(csmaNodes.Get(1), 30, 10);
  anim.SetConstantPosition(csmaNodes.Get(2), 50, 10);
  anim.SetConstantPosition(csmaNodes.Get(3), 70, 10);
  anim.SetConstantPosition(csmaNodes.Get(4), 40, 30);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}