#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install point-to-point net devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Install the internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create TCP server application
  uint16_t port = 8080;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create TCP client application
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
  onOffHelper.SetAttribute("DataRate", StringValue("83.886 Mbps")); //Adjusted datarate according to packet size and interval
  ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Configure the bulk send application to send a fixed number of packets
  Ptr<Application> app = clientApp.Get(0);
  Ptr<OnOffApplication> onOffApp = DynamicCast<OnOffApplication>(app);
  onOffApp->SetMaxBytes(1024 * 1000); // Send 1000 packets of 1024 bytes

  // Start the simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}