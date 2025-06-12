#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter(
      "TcpExample",
      INFO);  // Enable logging for this component (optional)

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(nodes);

  // Create TCP server application
  uint16_t port = 8080;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create TCP client application
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time interPacketInterval = Seconds(1.0);
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  onOffHelper.SetAttribute("DataRate",
                           DataRateValue(packetSize * 8));  // Send packets as fast as possible

  ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(2.0 + numPackets * interPacketInterval.GetSeconds()));

  // Start simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}