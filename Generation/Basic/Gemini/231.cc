#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("TcpClientServerMinimal"); // No commentary allowed, so comment this out or remove

int main (int argc, char *argv[])
{
  // Set up logging for components to see progress and reception
  // LogComponentEnable ("TcpClientServerMinimal", LOG_LEVEL_INFO); // No commentary allowed, so comment this out or remove
  ns3::LogComponentEnable ("PacketSink", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("OnOffApplication", ns3::LOG_LEVEL_INFO);

  // 1. Create nodes
  ns3::NodeContainer nodes;
  nodes.Create (2);

  ns3::Ptr<ns3::Node> clientNode = nodes.Get (0);
  ns3::Ptr<ns3::Node> serverNode = nodes.Get (1);

  // 2. Create and install a point-to-point link
  ns3::PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", ns3::StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));
  ns3::NetDeviceContainer devices = p2p.Install (nodes);

  // 3. Install the Internet stack on the nodes
  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  // 4. Assign IP addresses
  ns3::Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  ns3::Ipv4Address serverIp = interfaces.GetAddress (1); // IP address of the server node
  uint16_t port = 9; // Common discard port

  // 5. Setup Server Application (PacketSink)
  ns3::PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                    ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), port));
  ns3::ApplicationContainer serverApp = sinkHelper.Install (serverNode);
  serverApp.Start (ns3::Seconds (0.5)); // Server starts listening
  serverApp.Stop (ns3::Seconds (5.0));

  // 6. Setup Client Application (OnOffApplication)
  ns3::OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                                 ns3::InetSocketAddress (serverIp, port));
  // Send a small amount of data (e.g., 1KB)
  clientHelper.SetAttribute ("DataRate", ns3::DataRateValue (ns3::DataRate ("1Mbps")));
  clientHelper.SetAttribute ("PacketSize", ns3::UintegerValue (1024)); // 1KB packets
  clientHelper.SetAttribute ("MaxBytes", ns3::UintegerValue (1024)); // Send total 1KB data
  // Ensure the client sends continuously until MaxBytes is reached
  clientHelper.SetAttribute ("OnTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ns3::ApplicationContainer clientApp = clientHelper.Install (clientNode);
  clientApp.Start (ns3::Seconds (1.0)); // Client starts sending after server is up
  clientApp.Stop (ns3::Seconds (4.0));

  // 7. Run the simulation
  ns3::Simulator::Stop (ns3::Seconds (5.0));
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}